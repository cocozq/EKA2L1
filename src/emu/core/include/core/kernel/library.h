#pragma once

#include <core/kernel/kernel_obj.h>

#include <memory>
#include <optional>

namespace eka2l1 {
    namespace loader {
        struct romimg;
        struct eka2img;

        using e32img_ptr = std::shared_ptr<eka2img>;
        using romimg_ptr = std::shared_ptr<romimg>;
    }

    namespace kernel {
        class library : public kernel_obj {
            union {
                loader::romimg_ptr rom_img;
                loader::e32img_ptr e32_img;
            };

            enum {
                rom_img_library,
                e32_img_library
            } lib_type;

        public:
            library(kernel_system *kern, const std::string &lib_name, loader::romimg_ptr img);
            library(kernel_system *kern, const std::string &lib_name, loader::e32img_ptr img);

            std::optional<uint32_t> get_ordinal_address(const uint8_t idx);
        };
    }
}