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
    std::cout << "[*] Scanning TaskScheduler for Jobs vector (0x10 - 0x500)...\n";
    mach_vm_address_t jobs_begin = 0, jobs_end = 0;
    uint32_t jobs_offset = 0;
    bool is_shared_ptr_vector = false;

    for (uint32_t offset = 0x10; offset <= 0x500; offset += 8) {
        mach_vm_address_t begin = read_ptr(task, task_scheduler + offset);
        mach_vm_address_t end = read_ptr(task, task_scheduler + offset + 8);
        mach_vm_address_t cap = read_ptr(task, task_scheduler + offset + 16);

        if (begin != 0 && end > begin && cap >= end) {
            uint64_t bytes_diff = end - begin;
            
            // Job vector usually has ~10 to 50 jobs. 
            // If raw ptr: bytes_diff = jobs * 8. If shared_ptr: bytes_diff = jobs * 16.
            if (bytes_diff > 0 && bytes_diff < 0x1000) {
                mach_vm_address_t first_job = read_ptr(task, begin);
                // Heuristic: The first job is a pointer to the heap or executable memory
                if (first_job > 0x1000) {
                    std::cout << "  [?] Found potential vector at offset 0x" << std::hex << offset << std::dec 
                              << " (bytes: " << bytes_diff << ", first_ptr: 0x" << std::hex << first_job << std::dec << ")\n";

                    // Let's read the name of the first job to see if it makes sense (e.g. "Arbiter", "Render")
                    bool looks_like_job = false;
                    for (uint32_t name_off = 0x10; name_off <= 0x100; name_off += 8) {
                        std::string s = read_string(task, first_job + name_off);
                        if (s == "Arbiter" || s == "Render" || s.find("Job") != std::string::npos || s == "WaitingHybridScriptsJob") {
                            looks_like_job = true;
                            break;
                        }
                    }

                    // If it's a shared_ptr, the raw ptr is at begin, but the next job is at begin + 16
                    if (!looks_like_job) {
                        mach_vm_address_t first_job_shared = read_ptr(task, begin + 16);
                        if (first_job_shared > 0x1000) {
                             for (uint32_t name_off = 0x10; name_off <= 0x100; name_off += 8) {
                                std::string s = read_string(task, first_job_shared + name_off);
                                if (s == "Arbiter" || s == "Render" || s.find("Job") != std::string::npos || s == "WaitingHybridScriptsJob") {
                                    looks_like_job = true;
                                    is_shared_ptr_vector = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (looks_like_job) {
                        jobs_begin = begin;
                        jobs_end = end;
                        jobs_offset = offset;
                        std::cout << "  [+] Confirmed! This is the Jobs vector.\n";
                        break;
                    }
                }
            }
        }
    }

    if (jobs_begin == 0) {
        std::cout << "[-] Failed to find Jobs vector in TaskScheduler.\n";
        return 1;
    }

    std::cout << "[+] Jobs Vector at TaskScheduler + 0x" << std::hex << jobs_offset << "\n";
    
    uint64_t job_size = is_shared_ptr_vector ? 16 : 8;
    uint64_t job_count = (jobs_end - jobs_begin) / job_size;
    std::cout << "[+] Found " << std::dec << job_count << " jobs (element size: " << job_size << " bytes).\n";

    // 2. Locate Target Jobs and scan them deeply
    mach_vm_address_t whs_job = 0;
    mach_vm_address_t render_job = 0;

    for (uint64_t i = 0; i < job_count; i++) {
        mach_vm_address_t job_ptr_addr = jobs_begin + (i * job_size);
        mach_vm_address_t job = read_ptr(task, job_ptr_addr);
        
        for (uint32_t offset = 0x10; offset <= 0x100; offset += 8) {
            std::string s = read_string(task, job + offset);
            if (s == "WaitingHybridScriptsJob") whs_job = job;
            if (s == "Render") render_job = job; // Sometimes just "Render"
            if (s == "RenderJob") render_job = job;
        }
    }

    std::cout << "[+] WaitingHybridScriptsJob: 0x" << std::hex << whs_job << "\n";
    std::cout << "[+] RenderJob: 0x" << std::hex << render_job << "\n";

    // 3. Dynamic Stats -> DataModel Target Dumper
    mach_vm_address_t datamodel = 0;
    mach_vm_address_t workspace = 0;
    uint32_t instance_name_offset = 0;
    
    mach_vm_address_t stats_obj = 0;
    std::cout << "[*] Scanning all jobs dynamically for Stats object...\n";
    for (uint64_t i = 0; i < job_count; i++) {
        mach_vm_address_t job_ptr_addr = jobs_begin + (i * job_size);
        mach_vm_address_t job = read_ptr(task, job_ptr_addr);
        
        for (uint32_t j_off = 0x10; j_off <= 0x200; j_off += 8) {
            mach_vm_address_t ptr = read_ptr(task, job + j_off);
            if (ptr > 0x100000000 && ptr < 0x7FFFFFFFFFFF) {
                for (uint32_t str_off = 0x10; str_off <= 0x80; str_off += 8) {
                    std::string s = read_string(task, ptr + str_off);
                    if (s == "(In - Out) KBps:" || s == "Instance count:") {
                        stats_obj = ptr;
                        std::cout << "[+] Found Stats object at Job + 0x" << std::hex << j_off << " -> 0x" << stats_obj << "\n";
                        break;
                    }
                }
            }
            if (stats_obj) break;
        }
        if (stats_obj) break;
    }
    
    if (stats_obj) {
        std::cout << "[*] Scanning Stats object for Parent (DataModel) candidates...\n";
        for (uint32_t p_off = 0x20; p_off <= 0x80; p_off += 8) {
            mach_vm_address_t parent_cand = read_ptr(task, stats_obj + p_off);
            if (parent_cand > 0x100000000 && parent_cand < 0x7FFFFFFFFFFF) {
                
                // Check if parent_cand has a children vector
                for (uint32_t v_off = 0x30; v_off <= 0x100; v_off += 8) {
                    mach_vm_address_t begin = read_ptr(task, parent_cand + v_off);
                    mach_vm_address_t end = read_ptr(task, parent_cand + v_off + 8);
                    mach_vm_address_t cap = read_ptr(task, parent_cand + v_off + 16);
                    
                    if (begin > 0x100000000 && begin < 0x7FFFFFFFFFFF && end > begin && cap >= end) {
                        uint64_t count = (end - begin) / 16;
                        if (count > 20 && count < 150) { // DataModel usually has ~40-100 children
                            std::cout << "  [+] Found candidate DataModel at Stats + 0x" << std::hex << p_off << " -> 0x" << parent_cand << "\n";
                            std::cout << "      -> Vector offset: 0x" << v_off << " | Child count: " << std::dec << count << "\n";
                            
                            // Dump strings of the first few children to identify them
                            std::cout << "      [*] Dumping strings for first 15 children...\n";
                            for (uint64_t i = 0; i < std::min<uint64_t>(15, count); i++) {
                                mach_vm_address_t child = read_ptr(task, begin + (i * 16));
                                if (child > 0x100000000) {
                                    std::cout << "          -> Child " << i << " (0x" << std::hex << child << ")\n";
                                    for (uint32_t str_off = 0x28; str_off <= 0x68; str_off += 8) {
                                        std::string name = read_string(task, child + str_off);
                                        if (name.length() >= 3 && name.length() < 30) {
                                            bool printable = true;
                                            for (char c : name) { if (c < 32 || c > 126) printable = false; }
                                            if (printable) {
                                                std::cout << "               [String @ 0x" << std::hex << str_off << "]: " << name << "\n";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (datamodel != 0) {
        std::cout << "[+] Extracted DataModel: 0x" << std::hex << datamodel << "\n";
        
        // Scan DataModel for Children offset
        uint32_t children_offset = 0;
        mach_vm_address_t children_begin = 0, children_end = 0;

        for (uint32_t off = 0x30; off <= 0x100; off += 8) {
            mach_vm_address_t begin = read_ptr(task, datamodel + off);
            mach_vm_address_t end = read_ptr(task, datamodel + off + 8);
            mach_vm_address_t cap = read_ptr(task, datamodel + off + 16);

            if (begin != 0 && end > begin && cap >= end) {
                uint64_t count = (end - begin) / 16; // shared_ptr is 16 bytes
                if (count > 0 && count < 200) {
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

            mach_vm_address_t players = 0;
            if (workspace == 0) {
                // Find workspace and players
                for (uint64_t i = 0; i < count; i++) {
                    mach_vm_address_t child_ptr = read_ptr(task, children_begin + (i * 16));
                    if (child_ptr) {
                        std::string child_name = read_string(task, child_ptr + instance_name_offset);
                        if (!child_name.empty()) {
                            std::cout << "  -> Child: " << child_name << " (0x" << std::hex << child_ptr << ")\n";
                            if (child_name == "Workspace") workspace = child_ptr;
                            if (child_name == "Players") players = child_ptr;
                        }
                    }
                }
            } else {
                for (uint64_t i = 0; i < count; i++) {
                    mach_vm_address_t child_ptr = read_ptr(task, children_begin + (i * 16));
                    if (child_ptr) {
                        std::string child_name = read_string(task, child_ptr + instance_name_offset);
                        if (child_name == "Players") {
                            players = child_ptr;
                            std::cout << "  -> Child: " << child_name << " (0x" << std::hex << child_ptr << ")\n";
                        }
                    }
                }
            }

            if (players != 0) {
                std::cout << "[+] Found Players: 0x" << std::hex << players << "\n";
                uint32_t localplayer_offset = 0;
                mach_vm_address_t localplayer = 0;
                
                for (uint32_t off = 0x50; off <= 0x1A0; off += 8) {
                    mach_vm_address_t ptr = read_ptr(task, players + off);
                    if (ptr > 0x100000000 && ptr < 0x7FFFFFFFFFFF) {
                        for (uint32_t p_off = 0x30; p_off <= 0x80; p_off += 8) {
                            if (read_ptr(task, ptr + p_off) == players) {
                                std::string lp_name = read_string(task, ptr + instance_name_offset);
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
