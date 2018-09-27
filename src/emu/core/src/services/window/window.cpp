/*
 * Copyright (c) 2018 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <core/services/window/op.h>
#include <core/services/window/window.h>

#include <common/log.h>

#include <core/core.h>
#include <optional>
#include <string>

#include <core/drivers/screen_driver.h>

namespace eka2l1::epoc {
    window_client_obj::window_client_obj(window_server_client_ptr client)
        : client(client) {
    }

    screen_device::screen_device(window_server_client_ptr client, eka2l1::driver::screen_driver_ptr driver)
        : window_client_obj(client)
        , driver(driver) {
    }

    void screen_device::execute_command(eka2l1::service::ipc_context ctx, eka2l1::ws_cmd cmd) {
        TWsScreenDeviceOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsSdOpPixelSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = driver->get_window_size();
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpTwipsSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = driver->get_window_size();
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size * 15);
            ctx.set_request_status(0);

            break;
        }

        default: {
            LOG_WARN("Unimplemented IPC call for screen driver: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }

    void window_server_client::parse_command_buffer(service::ipc_context ctx) {
        std::optional<std::string> dat = ctx.get_arg<std::string>(cmd_slot);

        if (!dat) {
            return;
        }

        char *beg = dat->data();
        char *end = dat->data() + dat->size();

        std::vector<ws_cmd> cmds;

        while (beg < end) {
            ws_cmd cmd;

            cmd.header = *reinterpret_cast<ws_cmd_header *>(beg);

            if (cmd.header.op & 0x8000) {
                cmd.header.op &= ~0x8000;
                beg += sizeof(ws_cmd_header) + sizeof(cmd.obj_handle);
            } else {
                beg += sizeof(ws_cmd_header);
            }

            cmd.data_ptr = reinterpret_cast<void *>(beg);
            beg += cmd.header.cmd_len;

            cmds.push_back(std::move(cmd));
        }

        execute_commands(ctx, std::move(cmds));
    }

    window_server_client::window_server_client(session_ptr guest_session)
        : guest_session(guest_session) {
        root = std::make_shared<epoc::window>(this);       
    }

    void window_server_client::execute_commands(service::ipc_context ctx, std::vector<ws_cmd> cmds) {
        for (const auto &cmd : cmds) {
            if (!cmd.obj_handle || cmd.obj_handle > objects.size()) {
                LOG_WARN("Object handle is invalid {}", cmd.obj_handle);
                continue;
            }

            objects[cmd.obj_handle - 1]->execute_command(ctx, cmd);
        }
    }

    void window_server_client::create_screen_device(service::ipc_context ctx, ws_cmd cmd) {
        LOG_INFO("Create screen device.");

        ws_cmd_screen_device_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);

        epoc::screen_device_ptr device
            = std::make_shared<epoc::screen_device>(
                this, ctx.sys->get_screen_driver());

        if (!primary_device) {
            primary_device = device;
        }

        auto id = device->id;

        init_device(root);
        ctx.set_request_status(id);
    }

    void window_server_client::init_device(epoc::window_ptr &win) {
        if (win->type == epoc::window_type::group) {
            epoc::window_group_ptr group_win = std::reinterpret_pointer_cast<epoc::window_group>(win);

            if (!group_win->dvc) {
                group_win->dvc = primary_device;
            }
        }

        for (auto &child_win : win->childs) {
            init_device(child_win);
        }
    }

    void window_server_client::restore_hotkey(service::ipc_context ctx, ws_cmd cmd) {
        THotKey key = *reinterpret_cast<THotKey *>(cmd.data_ptr);

        LOG_WARN("Unknown restore key op.");
    }

    void window_server_client::create_window_group(service::ipc_context ctx, ws_cmd cmd) {
        ws_cmd_window_group_header *header = reinterpret_cast<decltype(header)>(cmd.data_ptr);
        int device_handle = header->screen_device_handle;

        epoc::screen_device_ptr device_ptr;

        if (device_handle <= 0) {
            device_ptr = primary_device;
        } else {
            device_ptr = std::dynamic_pointer_cast<epoc::screen_device>(objects[device_handle - 1]);
        }

        epoc::window_ptr group = std::make_shared<epoc::window_group>(this, device_ptr);
        epoc::window_ptr parent_group = find_window_obj(root, header->parent_id);

        if (!parent_group) {
            LOG_WARN("Unable to find parent for new group with ID = 0x{:x}. Use root", header->parent_id);
            parent_group = root;
        }

        const uint32_t gid = group->id;

        group->parent = parent_group;
        parent_group->childs.push(std::move(group));

        ctx.set_request_status(gid);
    }

    epoc::window_ptr window_server_client::find_window_obj(epoc::window_ptr &root, std::uint32_t id) {
        if (root->id == id) {
            return root;
        }

        if (root->childs.size() == 0) {
            return nullptr;
        }

        for (auto &child_win : root->childs) {
            epoc::window_ptr obj = find_window_obj(child_win, id);

            if (obj) {
                return obj;
            }
        }

        return nullptr;
    }

    // This handle both sync and async
    void window_server_client::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        switch (cmd.header.op) {
        case EWsClOpCreateScreenDevice:
            create_screen_device(ctx, cmd);
            break;

        case EWsClOpCreateWindowGroup:
            create_window_group(ctx, cmd);
            break;

        case EWsClOpRestoreDefaultHotKey:
            restore_hotkey(ctx, cmd);
            break;

        case EWsClOpEventReady:
            break;

        default:
            LOG_INFO("Unimplemented ClOp: 0x{:x}", cmd.header.op);
        }
    }
}

namespace eka2l1 {
    window_server::window_server(system *sys)
        : service::server(sys, "!Windowserver", true) {
        REGISTER_IPC(window_server, init, EWservMessInit,
            "Ws::Init");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessCommandBuffer,
            "Ws::CommandBuffer");
        REGISTER_IPC(window_server, send_to_command_buffer, EWservMessSyncMsgBuf,
            "Ws::MessSyncBuf");
    }

    void window_server::init(service::ipc_context ctx) {
        clients.emplace(ctx.msg->msg_session->unique_id(), std::make_shared<epoc::window_server_client>(ctx.msg->msg_session));
        ctx.set_request_status(ctx.msg->msg_session->unique_id());
    }

    void window_server::send_to_command_buffer(service::ipc_context ctx) {
        clients[ctx.msg->msg_session->unique_id()]->parse_command_buffer(ctx);
    }
}