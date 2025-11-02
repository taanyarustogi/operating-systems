#include "vms.h"

#include "mmu.h"
#include "pages.h"

#include <string.h>

static int count[MAX_PAGES] = {0};

static void print_pte_entry(uint64_t* entry) {
    const char* dash = "-";
    const char* custom = dash;
    const char* write = dash;
    const char* read = dash;
    const char* valid = dash;
    if (vms_pte_custom(entry)) {
        custom = "C";
    }
    if (vms_pte_write(entry)) {
        write = "W";
    }
    if (vms_pte_read(entry)) {
        read = "R";
    }
    if (vms_pte_valid(entry)) {
        valid = "V";
    }

    printf("PPN: 0x%lX Flags: %s%s%s%s\n",
        vms_pte_get_ppn(entry),
        custom, write, read, valid);
} //debugging function

void page_fault_handler(void* virtual_address, int level, void* page_table) {
    if (level == 0) {
        uint64_t* entry = vms_page_table_pte_entry(page_table, virtual_address, level);

        if (vms_pte_custom(entry) == 0) return;

        if (vms_pte_write(entry) == 0) {
            uint64_t entry_ppn = vms_pte_get_ppn(entry);
            int entry_index = vms_get_page_index(vms_ppn_to_page(entry_ppn)); //get entry index

            if (count[entry_index] > 0) { //if more than one references
                void* entry_copy = vms_new_page(); //create a copy 
                memcpy(entry_copy, vms_ppn_to_page(vms_pte_get_ppn(entry)), PAGE_SIZE);
                vms_pte_set_ppn(entry, vms_page_to_ppn(entry_copy)); 
                count[entry_index]--;
            }

            vms_pte_write_set(entry); //enable
            vms_pte_custom_clear(entry); //clear custom
        }
    }
}

void* vms_fork_copy() {
    void* parent_l2 = vms_get_root_page_table();
    void* child_l2 = vms_new_page();

    for(int i = 0; i < NUM_PTE_ENTRIES; i++) {
        uint64_t* entry_parent_l2 = vms_page_table_pte_entry_from_index(parent_l2, i);

        if(vms_pte_valid(entry_parent_l2)) { //find the index on L2
            void* child_l1 = vms_new_page();
            uint64_t child_l1_ppn = vms_page_to_ppn(child_l1); //get pnn for child L1 page
            uint64_t* entry_child_l2 = vms_page_table_pte_entry_from_index(child_l2,i);
            vms_pte_set_ppn(entry_child_l2, child_l1_ppn); //write L1 pnn to L2
            vms_pte_valid_set(entry_child_l2); //set valid bit

            uint64_t parent_l1_ppn = vms_pte_get_ppn(entry_parent_l2); //get L1 pnn for parent

            for(int j = 0; j < NUM_PTE_ENTRIES; j++) {
                uint64_t* entry_parent_l1 = vms_page_table_pte_entry_from_index(vms_ppn_to_page(parent_l1_ppn), j); //get entries on the L1 page

                if(vms_pte_valid(entry_parent_l1)) { //find index on L1
                    void* child_l0 = vms_new_page();
                    uint64_t child_l0_ppn = vms_page_to_ppn(child_l0); //get pnn for child L0 page
                    uint64_t* entry_child_l1 = vms_page_table_pte_entry_from_index(child_l1,j);
                    vms_pte_set_ppn(entry_child_l1, child_l0_ppn); //write L0 pnn to L1
                    vms_pte_valid_set(entry_child_l1); //set valid bit

                    uint64_t parent_l0_ppn = vms_pte_get_ppn(entry_parent_l1); //get L0 pnn for parent

                    for (int k = 0; k < NUM_PTE_ENTRIES; k++) {

                        uint64_t* entry_parent_l0 = vms_page_table_pte_entry_from_index(vms_ppn_to_page(parent_l0_ppn), k); //get entries on the L0 page

                        if(vms_pte_valid(entry_parent_l0)) { //find index on L0
                            //print_pte_entry(entry_child_l2);
                            void* child_p0 = vms_new_page();
                            uint64_t child_p0_ppn = vms_page_to_ppn(child_p0); //get pnn for child p0 page
                            uint64_t* entry_child_l0 = vms_page_table_pte_entry_from_index(child_l0,k);
                            vms_pte_set_ppn(entry_child_l0, child_p0_ppn); //write p0 pnn to L0
                            vms_pte_valid_set(entry_child_l0); //set valid bit
                            if(vms_pte_read(entry_parent_l0)) vms_pte_read_set(entry_child_l0); //set read bit
                            if(vms_pte_write(entry_parent_l0)) vms_pte_write_set(entry_child_l0); //set write bit

                            uint64_t parent_p0_ppn = vms_pte_get_ppn(entry_parent_l0); //get p0 pnn for parent
                            void* parent_p0 = vms_ppn_to_page(parent_p0_ppn); //get p0 parent page pointer

                           memcpy(child_p0, parent_p0, PAGE_SIZE); //copy p0 parent page to p0 child page
                        } 
                    } 
                }
            }
        }
    }
    return child_l2;
}

void* vms_fork_copy_on_write() {
    void* parent_l2 = vms_get_root_page_table();
    void* child_l2 = vms_new_page();

    for(int i = 0; i < NUM_PTE_ENTRIES; i++) {
        uint64_t* entry_parent_l2 = vms_page_table_pte_entry_from_index(parent_l2, i);

        if(vms_pte_valid(entry_parent_l2)) { //find the index on L2
            void* child_l1 = vms_new_page();
            uint64_t child_l1_ppn = vms_page_to_ppn(child_l1); //get pnn for child L1 page
            uint64_t* entry_child_l2 = vms_page_table_pte_entry_from_index(child_l2,i);
            vms_pte_set_ppn(entry_child_l2, child_l1_ppn); //write L1 pnn to L2
            vms_pte_valid_set(entry_child_l2); //set valid bit

            uint64_t parent_l1_ppn = vms_pte_get_ppn(entry_parent_l2); //get L1 pnn for parent

            for(int j = 0; j < NUM_PTE_ENTRIES; j++) {
                uint64_t* entry_parent_l1 = vms_page_table_pte_entry_from_index(vms_ppn_to_page(parent_l1_ppn), j); //get entries on the L1 page

                if(vms_pte_valid(entry_parent_l1)) { //find index on L1
                    void* child_l0 = vms_new_page();
                    uint64_t child_l0_ppn = vms_page_to_ppn(child_l0); //get pnn for child L0 page
                    uint64_t* entry_child_l1 = vms_page_table_pte_entry_from_index(child_l1,j);
                    vms_pte_set_ppn(entry_child_l1, child_l0_ppn); //write L0 pnn to L1
                    vms_pte_valid_set(entry_child_l1); //set valid bit

                    uint64_t parent_l0_ppn = vms_pte_get_ppn(entry_parent_l1); //get L0 pnn for parent

                    for (int k = 0; k < NUM_PTE_ENTRIES; k++) {

                        uint64_t* entry_parent_l0 = vms_page_table_pte_entry_from_index(vms_ppn_to_page(parent_l0_ppn), k); //get entries on the L0 page

                        if(vms_pte_valid(entry_parent_l0)) { //find index on L0
                            uint64_t parent_p0_ppn = vms_pte_get_ppn(entry_parent_l0); //get pnn for parent p0 page
                            uint64_t* entry_child_l0 = vms_page_table_pte_entry_from_index(child_l0,k);
                            vms_pte_set_ppn(entry_child_l0, parent_p0_ppn); //write p0 pnn to L0
                            vms_pte_valid_set(entry_child_l0); //set valid bit
                            if(vms_pte_read(entry_parent_l0)) vms_pte_read_set(entry_child_l0); //set read bit
                            if(vms_pte_write(entry_parent_l0)) {
                                vms_pte_custom_set(entry_child_l0); //set child custom bit
                                vms_pte_custom_set(entry_parent_l0); //set parent costum bit
                                vms_pte_write_clear(entry_parent_l0); //clear parents write bit 
                            }
                            if(vms_pte_custom(entry_parent_l0)) {
                                vms_pte_custom_set(entry_child_l0); //if parent costum bit, set child's
                            }
                        
                            count[vms_get_page_index(vms_ppn_to_page(parent_p0_ppn))]++; //track number of copies
                        }
                    } 
                }
            }
        }
    }
    return child_l2;
}
