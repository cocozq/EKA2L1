/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <mem/page.h>
#include <memory>

namespace eka2l1::arm {
    class core;
}

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::mem {
    constexpr std::size_t PAGE_SIZE_BYTES_12B = 0x1000;
    constexpr std::size_t PAGE_SIZE_BYTES_20B = 0x100000;

    enum {
        MMU_ASSIGN_LOCAL_GLOBAL_REGION = 1 << 0,
        MMU_ASSIGN_GLOBAL = 1 << 1
    };

    /**
     * \brief The base of memory management unit.
     */
    class mmu_base {
    protected:
        page_table_allocator *alloc_; ///< Page table allocator.

        bool read_8bit_data(const vm_address addr, std::uint8_t *data);
        bool read_16bit_data(const vm_address addr, std::uint16_t *data);
        bool read_32bit_data(const vm_address addr, std::uint32_t *data);
        bool read_64bit_data(const vm_address addr, std::uint64_t *data);

        bool write_8bit_data(const vm_address addr, std::uint8_t *data);
        bool write_16bit_data(const vm_address addr, std::uint16_t *data);
        bool write_32bit_data(const vm_address addr, std::uint32_t *data);
        bool write_64bit_data(const vm_address addr, std::uint64_t *data);

    public:
        std::size_t page_size_bits_; ///< The number of bits of page size.
        std::uint32_t offset_mask_;
        std::uint32_t page_index_mask_;
        std::uint32_t page_index_shift_;
        std::uint32_t page_table_index_shift_;
        std::uint32_t chunk_shift_;
        std::uint32_t chunk_size_;
        std::uint32_t chunk_mask_;
        std::uint32_t page_per_tab_shift_;

        bool mem_map_old_; ///< Should we use EKA1 mem map model?
        arm::core *cpu_;
        config::state *conf_;

    public:
        explicit mmu_base(page_table_allocator *alloc, arm::core *cpu, config::state *conf, std::size_t psize_bits = 10, const bool mem_map_old = false);

        virtual ~mmu_base() {}

        virtual const mem_model_type model_type() const = 0;

        void map_to_cpu(const vm_address addr, const std::size_t size, void *ptr, const prot perm);
        void unmap_from_cpu(const vm_address addr, const std::size_t size);

        /**
         * \brief Get number of bytes a page occupy
         */
        const std::size_t page_size() const {
            return page_size_bits_ == 12 ? PAGE_SIZE_BYTES_12B : PAGE_SIZE_BYTES_20B;
        }

        const bool using_old_mem_map() const {
            return mem_map_old_;
        }

        /**
         * \brief Get a page table by its ID.
         */
        page_table *get_page_table_by_id(const std::uint32_t id) {
            return alloc_->get_page_table_by_id(id);
        }

        /**
         * \brief Get host pointer of a virtual address, in the specified address space.
         */
        virtual void *get_host_pointer(const asid id, const vm_address addr) = 0;

        /**
         * \brief Create a new page table.
         * 
         * The new page table will not be assigned to any existing page directory, until
         * the user decide to assign an index to it using the MMU.
         * 
         * When we assign an index to it, it will be attached to the current page directory
         * the MMU is managing.
         */
        virtual page_table *create_new_page_table();

        /**
         * \brief Create or renew an address space if possible.
         * 
         * \returns An ASID identify the address space. -1 if a new one can't be create.
         */
        virtual asid rollover_fresh_addr_space() = 0;

        /**
         * \brief Set current MMU's address space.
         * 
         * \param id The ASID of the target address space
         */
        virtual bool set_current_addr_space(const asid id) = 0;

        virtual const asid current_addr_space() const = 0;

        /**
         * \brief Assign page tables at linear base address to page directories.
         */
        virtual void assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags, asid *id_list = nullptr, const std::uint32_t id_list_size = 0) = 0;
    };

    using mmu_impl = std::unique_ptr<mmu_base>;

    mmu_impl make_new_mmu(page_table_allocator *alloc, arm::core *cpu, config::state *conf, const std::size_t psize_bits, const bool mem_map_old,
        const mem_model_type model);
}
