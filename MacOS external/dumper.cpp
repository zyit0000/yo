#include <iostream>
#include <vector>
#include <string>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld_images.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <unistd.h>

// Helper to get PID by process name
pid_t get_pid_by_name(const char* name) {
    pid_t pids[2048];
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    int n_pids = bytes / sizeof(pids[0]);

    for (int i = 0; i < n_pids; i++) {
        struct proc_bsdinfo proc;
        int st = proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE);
        if (st == PROC_PIDTBSDINFO_SIZE) {
            if (strcmp(name, proc.pbi_name) == 0 || strstr(proc.pbi_comm, name) != nullptr) {
                return pids[i];
            }
        }
    }
    return 0;
}

// Helper to read memory
bool read_memory(mach_port_t task, mach_vm_address_t address, void* buffer, mach_vm_size_t size) {
    mach_vm_size_t read_size = 0;
    kern_return_t kr = mach_vm_read_overwrite(task, address, size, (mach_vm_address_t)buffer, &read_size);
    return (kr == KERN_SUCCESS && read_size == size);
}

// Read an integer (pointer size)
uintptr_t read_ptr(mach_port_t task, mach_vm_address_t address) {
    uintptr_t val = 0;
    read_memory(task, address, &val, sizeof(val));
    return val;
}

// Helper to read libc++ std::string
std::string read_string(mach_port_t task, mach_vm_address_t address) {
    char buf[24] = {0};
    if (!read_memory(task, address, buf, 24)) return "";

    // macOS libc++ std::string
    // if (buf[0] & 1) is true -> long string
    bool is_long = (buf[0] & 1);
    if (is_long) {
        mach_vm_address_t str_data = *(mach_vm_address_t*)(buf + 16); // data ptr is at offset 16
        mach_vm_size_t str_size = *(mach_vm_size_t*)(buf + 8);        // size is at offset 8
        if (str_size > 0 && str_size < 1000) {
            std::vector<char> long_buf(str_size + 1, 0);
            if (read_memory(task, str_data, long_buf.data(), str_size)) {
                return std::string(long_buf.data());
            }
        }
        return "";
    } else {
        // short string: size is at buf[0] >> 1
        int size = (unsigned char)buf[0] >> 1;
        if (size > 0 && size < 24) {
            return std::string(buf + 1, size);
        }
        return "";
    }
}

// Read raw string without std::string struct
std::string read_raw_string(mach_port_t task, mach_vm_address_t address, int max_len = 50) {
    std::string res = "";
    for (int i = 0; i < max_len; i++) {
        char c = 0;
        if (read_memory(task, address + i, &c, 1) && c != 0) {
            res += c;
        } else {
            break;
        }
    }
    return res;
}

int main() {
    std::cout << "[*] macOS External Dumper Starting...\n";

    pid_t pid = get_pid_by_name("RobloxPlayer");
    if (pid == 0) {
        std::cout << "[-] RobloxPlayer not found. Is it running?\n";
        return 1;
    }
    std::cout << "[+] Found RobloxPlayer PID: " << pid << "\n";

    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        std::cout << "[-] task_for_pid failed (" << kr << "). Try running with sudo.\n";
        return 1;
    }

    task_dyld_info dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    kr = task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count);
    
    struct dyld_all_image_infos infos;
    read_memory(task, dyld_info.all_image_info_addr, &infos, sizeof(infos));
    struct dyld_image_info image;
    read_memory(task, (mach_vm_address_t)infos.infoArray, &image, sizeof(image));

    mach_vm_address_t base_address = (mach_vm_address_t)image.imageLoadAddress;
    std::cout << "[+] Roblox Base Address: 0x" << std::hex << base_address << std::dec << "\n";

    uintptr_t ts_offset = 0x657AFB0;
    mach_vm_address_t ts_ptr_addr = base_address + ts_offset;
    uintptr_t task_scheduler = read_ptr(task, ts_ptr_addr);
    
    if (task_scheduler == 0) {
        std::cout << "[-] TaskScheduler is null.\n";
        return 1;
    }
    std::cout << "[+] TaskScheduler Instance: 0x" << std::hex << task_scheduler << std::dec << "\n";

    // 1. Scan TaskScheduler for Jobs vector
    // A vector is [begin, end, cap]. We look for sensible values.
    std::cout << "[*] Scanning TaskScheduler for Jobs vector...\n";
    mach_vm_address_t jobs_begin = 0, jobs_end = 0;
    uint32_t jobs_offset = 0;

    for (uint32_t offset = 0x100; offset <= 0x200; offset += 8) {
        mach_vm_address_t begin = read_ptr(task, task_scheduler + offset);
        mach_vm_address_t end = read_ptr(task, task_scheduler + offset + 8);
        mach_vm_address_t cap = read_ptr(task, task_scheduler + offset + 16);

        if (begin != 0 && end > begin && cap >= end) {
            uint64_t count = (end - begin) / 8; // pointer size is 8
            if (count > 0 && count < 100) {
                // heuristic: verify if the first pointer points to valid memory
                mach_vm_address_t first_job = read_ptr(task, begin);
                if (first_job > 0x100000000) { // arbitrary valid pointer check
                    jobs_begin = begin;
                    jobs_end = end;
                    jobs_offset = offset;
                    break;
                }
            }
        }
    }

    if (jobs_begin == 0) {
        std::cout << "[-] Failed to find Jobs vector in TaskScheduler.\n";
        return 1;
    }

    std::cout << "[+] Jobs Vector at TaskScheduler + 0x" << std::hex << jobs_offset << "\n";
    
    uint64_t job_count = (jobs_end - jobs_begin) / 8;
    std::cout << "[+] Found " << std::dec << job_count << " jobs.\n";

    // 2. Iterate Jobs to find Name offset and specific jobs
    mach_vm_address_t datamodel = 0;
    uint32_t job_name_offset = 0;

    for (uint64_t i = 0; i < job_count; i++) {
        mach_vm_address_t job_ptr_addr = jobs_begin + (i * 8);
        mach_vm_address_t job = read_ptr(task, job_ptr_addr);
        
        // Find job name offset by scanning job memory
        std::string job_name = "";
        for (uint32_t offset = 0x10; offset <= 0x100; offset += 8) {
            std::string s = read_string(task, job + offset);
            if (s == "WaitingHybridScriptsJob" || s == "Render" || s.find("Job") != std::string::npos) {
                job_name = s;
                job_name_offset = offset;
                break;
            }
        }

        if (!job_name.empty()) {
            std::cout << "  -> Job [0x" << std::hex << job << "]: " << job_name << " (Name offset: 0x" << job_name_offset << ")\n";
            
            // WaitingHybridScriptsJob holds ScriptContext -> DataModel
            if (job_name == "WaitingHybridScriptsJob") {
                // We'll scan WaitingHybridScriptsJob for pointers to ScriptContext/DataModel
                // DataModel has a specific Name ("Game") and children.
                std::cout << "     [*] Scanning WaitingHybridScriptsJob for DataModel/ScriptContext...\n";
                for (uint32_t j_off = 0x10; j_off <= 0x300; j_off += 8) {
                    mach_vm_address_t ptr = read_ptr(task, job + j_off);
                    if (ptr > 0x100000000) {
                        // Scan ptr for "Game" string (DataModel name offset is usually 0x48)
                        std::string dm_name = read_string(task, ptr + 0x48);
                        if (dm_name == "Game" || dm_name == "Ugc") {
                            datamodel = ptr;
                            std::cout << "     [+] Found DataModel at Job + 0x" << std::hex << j_off << " -> 0x" << datamodel << "\n";
                            std::cout << "     [+] DataModel Name offset: 0x48\n";
                            break;
                        }
                    }
                }
            }
        }
    }

    if (datamodel != 0) {
        std::cout << "[+] Extracted DataModel: 0x" << std::hex << datamodel << "\n";
        
        // Scan DataModel for Children offset
        // Children is a std::vector<shared_ptr<Instance>>. Usually at offset 0x50, 0x58, 0x70
        uint32_t children_offset = 0;
        mach_vm_address_t children_begin = 0, children_end = 0;

        for (uint32_t off = 0x30; off <= 0x100; off += 8) {
            mach_vm_address_t begin = read_ptr(task, datamodel + off);
            mach_vm_address_t end = read_ptr(task, datamodel + off + 8);
            mach_vm_address_t cap = read_ptr(task, datamodel + off + 16);

            if (begin != 0 && end > begin && cap >= end) {
                uint64_t count = (end - begin) / 16; // shared_ptr is 16 bytes
                if (count > 0 && count < 200) {
                    // This is likely the children vector
                    children_offset = off;
                    children_begin = begin;
                    children_end = end;
                    break;
                }
            }
        }

        if (children_offset != 0) {
            std::cout << "[+] Instance::Children offset: 0x" << std::hex << children_offset << "\n";
            uint64_t count = (children_end - children_begin) / 16;
            std::cout << "[+] DataModel has " << std::dec << count << " children.\n";

            // Print children to find Workspace, Players
            mach_vm_address_t players = 0;
            mach_vm_address_t workspace = 0;

            for (uint64_t i = 0; i < count; i++) {
                // shared_ptr layout: [ptr, control_block]
                mach_vm_address_t child_ptr = read_ptr(task, children_begin + (i * 16));
                if (child_ptr) {
                    std::string child_name = read_string(task, child_ptr + 0x48); // Assuming name is 0x48
                    if (!child_name.empty()) {
                        std::cout << "  -> Child: " << child_name << " (0x" << std::hex << child_ptr << ")\n";
                        if (child_name == "Workspace") workspace = child_ptr;
                        if (child_name == "Players") players = child_ptr;
                    }
                }
            }

            if (players != 0) {
                std::cout << "[+] Found Players: 0x" << std::hex << players << "\n";
                // LocalPlayer is usually an explicit pointer in Players (offset 0x100-0x150)
                // Let's scan Players for a valid pointer that points to an Instance with a valid Name
                uint32_t localplayer_offset = 0;
                mach_vm_address_t localplayer = 0;
                
                for (uint32_t off = 0x50; off <= 0x150; off += 8) {
                    mach_vm_address_t ptr = read_ptr(task, players + off);
                    if (ptr > 0x100000000) {
                        // Check if it has a Parent pointer pointing back to Players
                        // Parent is usually at offset 0x40, 0x60, etc.
                        for (uint32_t p_off = 0x30; p_off <= 0x70; p_off += 8) {
                            if (read_ptr(task, ptr + p_off) == players) {
                                std::string lp_name = read_string(task, ptr + 0x48);
                                if (!lp_name.empty()) {
                                    localplayer = ptr;
                                    localplayer_offset = off;
                                    std::cout << "[+] Found LocalPlayer (" << lp_name << ") at Players + 0x" << std::hex << localplayer_offset << "\n";
                                    std::cout << "[+] Instance::Parent offset: 0x" << p_off << "\n";
                                    break;
                                }
                            }
                        }
                    }
                    if (localplayer != 0) break;
                }
            }
        } else {
            std::cout << "[-] Failed to find Instance::Children offset.\n";
        }
    } else {
        std::cout << "[-] Failed to find DataModel.\n";
    }

    std::cout << "[+] Done.\n";

    return 0;
}
