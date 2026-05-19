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

    // 3. Absolute Brute-Force DataModel & Permanent Offset Resolver
    mach_vm_address_t datamodel = 0;
    uint32_t offset_children = 0;
    uint32_t offset_name = 0;
    mach_vm_address_t players_svc = 0;
    
    std::cout << "[*] Commencing deep-scan heuristics for DataModel via Children vector...\n";
    for (uint64_t i = 0; i < job_count; i++) {
        mach_vm_address_t job = read_ptr(task, jobs_begin + (i * job_size));
        
        for (uint32_t j_off = 0x10; j_off <= 0x300; j_off += 8) {
            mach_vm_address_t ptr1 = read_ptr(task, job + j_off);
            if (ptr1 > 0x100000000 && ptr1 < 0x7FFFFFFFFFFF) {
                
                // Scan ptr1 directly
                mach_vm_address_t cands[2] = { ptr1, 0 };
                for (uint32_t p_off = 0x10; p_off <= 0x100; p_off += 8) {
                    mach_vm_address_t ptr2 = read_ptr(task, ptr1 + p_off);
                    if (ptr2 > 0x100000000 && ptr2 < 0x7FFFFFFFFFFF) {
                        cands[1] = ptr2;
                        
                        for (int k = 0; k < 2; k++) {
                            mach_vm_address_t cand = cands[k];
                            
                            for (uint32_t v_off = 0x10; v_off <= 0x200; v_off += 8) {
                                mach_vm_address_t begin = read_ptr(task, cand + v_off);
                                mach_vm_address_t end = read_ptr(task, cand + v_off + 8);
                                mach_vm_address_t cap = read_ptr(task, cand + v_off + 16);
                                
                                if (begin > 0x100000000 && begin < 0x7FFFFFFFFFFF && end > begin && cap >= end) {
                                    uint64_t count = (end - begin) / 16;
                                    if (count >= 30 && count <= 150) {
                                        
                                        int service_matches = 0;
                                        uint32_t found_name_off = 0;
                                        
                                        for (uint64_t c = 0; c < std::min<uint64_t>(30, count); c++) {
                                            mach_vm_address_t child = read_ptr(task, begin + (c * 16));
                                            if (child > 0x100000000) {
                                                for (uint32_t s_off = 0x10; s_off <= 0xA0; s_off += 8) {
                                                    std::string s = read_string(task, child + s_off);
                                                    if (s == "Workspace" || s == "Players" || s == "Lighting" || s == "ReplicatedStorage" || s == "CoreGui") {
                                                        service_matches++;
                                                        found_name_off = s_off;
                                                        if (s == "Players") players_svc = child;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                        
                                        if (service_matches >= 3) {
                                            datamodel = cand;
                                            offset_children = v_off;
                                            offset_name = found_name_off;
                                            std::cout << "  [+] BINGO! DataModel Confirmed: 0x" << std::hex << datamodel << "\n";
                                            break;
                                        }
                                    }
                                }
                            }
                            if (datamodel) break;
                        }
                        if (datamodel) break;
                    }
                }
                if (datamodel) break;
            }
            if (datamodel) break;
        }
        if (datamodel) break;
    }
    
    uint32_t offset_localplayer = 0;
    mach_vm_address_t localplayer = 0;
    
    if (players_svc) {
        std::cout << "[*] Scanning Players service (0x" << std::hex << players_svc << ") for LocalPlayer...\n";
        for (uint32_t lp_off = 0x10; lp_off <= 0x200; lp_off += 8) {
            mach_vm_address_t lp_cand = read_ptr(task, players_svc + lp_off);
            if (lp_cand > 0x100000000 && lp_cand < 0x7FFFFFFFFFFF) {
                // Check if lp_cand is an Instance by reading its name
                std::string lp_name = read_string(task, lp_cand + offset_name);
                if (lp_name.length() >= 3 && lp_name.length() <= 20) {
                    bool printable = true;
                    for (char c : lp_name) { if (c < 32 || c > 126) printable = false; }
                    if (printable) {
                        // Very likely the LocalPlayer
                        std::cout << "  [+] Found LocalPlayer candidate: '" << lp_name << "' at offset 0x" << std::hex << lp_off << "\n";
                        offset_localplayer = lp_off;
                        localplayer = lp_cand;
                        break;
                    }
                }
            }
        }
    }

    uint32_t offset_class_desc = 0;
    uint32_t offset_class_name = 0;
    uint32_t offset_parent = 0;
    mach_vm_address_t workspace = 0;
    
    if (datamodel != 0) {
        // Find Workspace to resolve ClassDescriptor
        mach_vm_address_t children_begin = read_ptr(task, datamodel + offset_children);
        mach_vm_address_t children_end = read_ptr(task, datamodel + offset_children + 8);
        uint64_t count = (children_end - children_begin) / 16;
        
        for (uint64_t i = 0; i < count; i++) {
            mach_vm_address_t child = read_ptr(task, children_begin + (i * 16));
            if (child) {
                std::string s = read_string(task, child + offset_name);
                if (s == "Workspace") {
                    workspace = child;
                    break;
                }
            }
        }
        
        if (workspace) {
            // 1. Resolve ClassDescriptor and Parent
            for (uint32_t p_off = 0x10; p_off <= 0x80; p_off += 8) {
                if (read_ptr(task, workspace + p_off) == datamodel) {
                    offset_parent = p_off;
                }
            }
            
            for (uint32_t c_off = 0x10; c_off <= 0x40; c_off += 8) {
                mach_vm_address_t cdesc = read_ptr(task, workspace + c_off);
                if (cdesc > 0x100000000 && cdesc < 0x7FFFFFFFFFFF) {
                    for (uint32_t s_off = 0x8; s_off <= 0x30; s_off += 8) {
                        if (read_string(task, cdesc + s_off) == "Workspace") {
                            offset_class_desc = c_off;
                            offset_class_name = s_off;
                            break;
                        }
                    }
                }
            }
        }
    }

    uint32_t humanoid_health = 0, humanoid_maxhealth = 0, humanoid_walkspeed = 0, humanoid_jumppower = 0;
    uint32_t part_size = 0, part_cframe = 0;
    
    auto read_float = [&](mach_vm_address_t addr) -> float {
        float val = 0;
        mach_vm_size_t read_size = 0;
        mach_vm_read_overwrite(task, addr, sizeof(float), (mach_vm_address_t)&val, &read_size);
        return val;
    };

    if (workspace && offset_class_desc) {
        std::cout << "[*] Scanning Workspace for BasePart and Humanoid...\n";
        mach_vm_address_t children_begin = read_ptr(task, workspace + offset_children);
        mach_vm_address_t children_end = read_ptr(task, workspace + offset_children + 8);
        uint64_t count = (children_end - children_begin) / 16;
        
        for (uint64_t i = 0; i < count; i++) {
            mach_vm_address_t model = read_ptr(task, children_begin + (i * 16));
            if (!model) continue;
            
            mach_vm_address_t m_desc = read_ptr(task, model + offset_class_desc);
            if (!m_desc) continue;
            std::string m_class = read_string(task, m_desc + offset_class_name);
            
            if (m_class == "Model") {
                // Scan Model children for Humanoid and Parts
                mach_vm_address_t m_c_begin = read_ptr(task, model + offset_children);
                mach_vm_address_t m_c_end = read_ptr(task, model + offset_children + 8);
                uint64_t m_count = (m_c_end - m_c_begin) / 16;
                if (m_count > 500) continue; // safety
                
                for (uint64_t j = 0; j < m_count; j++) {
                    mach_vm_address_t child = read_ptr(task, m_c_begin + (j * 16));
                    if (!child) continue;
                    
                    mach_vm_address_t c_desc = read_ptr(task, child + offset_class_desc);
                    if (!c_desc) continue;
                    std::string c_class = read_string(task, c_desc + offset_class_name);
                    
                    if (c_class == "Humanoid" && humanoid_walkspeed == 0) {
                        std::cout << "  [+] Found Humanoid at 0x" << std::hex << child << "\n";
                        for (uint32_t off = 0x100; off <= 0x300; off += 4) {
                            float val = read_float(child + off);
                            if (val == 16.0f) humanoid_walkspeed = off;
                            if (val == 50.0f) humanoid_jumppower = off;
                            if (val == 100.0f) {
                                if (humanoid_health == 0) humanoid_health = off;
                                else if (humanoid_maxhealth == 0 && off != humanoid_health) humanoid_maxhealth = off;
                            }
                        }
                    } else if ((c_class == "Part" || c_class == "MeshPart") && part_size == 0) {
                        std::string c_name = read_string(task, child + offset_name);
                        if (c_name == "HumanoidRootPart") {
                            std::cout << "  [+] Found HumanoidRootPart at 0x" << std::hex << child << "\n";
                            for (uint32_t off = 0x100; off <= 0x250; off += 4) {
                                float x = read_float(child + off);
                                float y = read_float(child + off + 4);
                                float z = read_float(child + off + 8);
                                if (x == 2.0f && y == 2.0f && z == 1.0f) {
                                    part_size = off;
                                }
                            }
                            // CFrame usually around 0xC0-0x150, look for identity matrix elements
                            for (uint32_t off = 0xA0; off <= 0x180; off += 4) {
                                float r00 = read_float(child + off);
                                float r11 = read_float(child + off + 16);
                                float r22 = read_float(child + off + 32);
                                if ((r00 == 1.0f || r00 == -1.0f) && (r11 == 1.0f || r11 == -1.0f) && (r22 == 1.0f || r22 == -1.0f)) {
                                    part_cframe = off;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            if (humanoid_walkspeed && part_size) break;
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "        PERMANENT OFFSETS DUMPED        \n";
    std::cout << "========================================\n\n";

    std::cout << "namespace Instance {\n";
    if (offset_name) std::cout << "    inline constexpr uintptr_t Name = 0x" << std::hex << offset_name << ";\n";
    if (offset_children) std::cout << "    inline constexpr uintptr_t Children = 0x" << std::hex << offset_children << ";\n";
    if (offset_parent) std::cout << "    inline constexpr uintptr_t Parent = 0x" << std::hex << offset_parent << ";\n";
    if (offset_class_desc) std::cout << "    inline constexpr uintptr_t ClassDescriptor = 0x" << std::hex << offset_class_desc << ";\n";
    std::cout << "}\n\n";

    std::cout << "namespace BasePart {\n";
    if (part_cframe) std::cout << "    inline constexpr uintptr_t CFrame = 0x" << std::hex << part_cframe << "; // Note: Position is CFrame + 0x24\n";
    if (part_size) std::cout << "    inline constexpr uintptr_t PartSize = 0x" << std::hex << part_size << ";\n";
    std::cout << "}\n\n";

    std::cout << "namespace Humanoid {\n";
    if (humanoid_health) std::cout << "    inline constexpr uintptr_t Health = 0x" << std::hex << humanoid_health << ";\n";
    if (humanoid_maxhealth) std::cout << "    inline constexpr uintptr_t MaxHealth = 0x" << std::hex << humanoid_maxhealth << ";\n";
    if (humanoid_walkspeed) std::cout << "    inline constexpr uintptr_t WalkSpeed = 0x" << std::hex << humanoid_walkspeed << ";\n";
    if (humanoid_jumppower) std::cout << "    inline constexpr uintptr_t JumpPower = 0x" << std::hex << humanoid_jumppower << ";\n";
    std::cout << "}\n\n";

    std::cout << "========================================\n\n";
    std::cout << "[+] Done.\n";

    return 0;
}
