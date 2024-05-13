#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <string.h>
#include <list>
#include <vector>

using namespace std;

const int PAS = 16;
const int page_size_Byte=256;
const int TLB_capacity=16;
const int num_pages=64;
const int num_pages_ram=64;
const int RAM_size_B=16384;
const int page_table_size=256;
const double addr_num=3000.0;

int tlb_hit=0, tlb_miss=0, read_count=0, page_fault_counter;

typedef struct TLB{
    int virtual_page_num;
    int frame_num;
}TLB;

typedef struct Frame{
    char frame[page_size_Byte];
}Frame;

typedef struct Page_tracker{
    int page_num;
    int r_bit;
}Page_tracker;

 int tlb_marker[TLB_capacity];
 int page_table[page_table_size];
 TLB tlb[TLB_capacity];
 Frame physical_mem[num_pages];
 list<Page_tracker> mem_tracker;
 char buffer[page_size_Byte];
 list<int> tlb_track;


int address_translation(int virtual_address, FILE * fd);
int checkTLB(int virtual_page_num);
int page_fault_handler(int virtual_page_num, bool partial , FILE * fd);
void update_TLB(int virtual_page_num, int frame_num, bool hit,  bool flag2 );
void header(void);
void display_log1(int address, int physical_address);
void display_log2(void);

int main()
{   
    freopen("log.txt", "w", stdout);
    memset(page_table, -1, page_table_size*sizeof(int));
    memset(tlb, -1, TLB_capacity*sizeof(tlb));
    FILE * fd= fopen("disk_sim", "rb");

    header();

   if(fd==NULL) printf("File descriptor is NULL");
   else
   {

        FILE* addr= fopen("addresses.txt", "r");
        if(addr!=NULL)
        {
            while(!feof(addr))
            {
                read_count++;
                int log_address=0;
                fscanf(addr, "%d", &log_address);
                int physical_address= address_translation(log_address, fd);

                display_log1(log_address, physical_address);

            }
                display_log2();
        }
        else printf("\nError opening addresses file\n");

   }

    fclose(fd);
    return 0;
}

int address_translation(int virtual_address, FILE * fd)
{
   
    int virtual_page_num=virtual_address/page_size_Byte;
    int offset= virtual_address%page_size_Byte;

    int result_TLB = checkTLB(virtual_page_num);

    if(result_TLB!=-1 && result_TLB!=-2) // TLB hit
    {
        tlb_hit++;
        int physical_address = result_TLB*page_size_Byte + offset;
        update_TLB(virtual_page_num, result_TLB, true, false);
        return physical_address;
    }
    else if(result_TLB==-2)
    {
         tlb_miss++;
         if(page_table[virtual_page_num]!=-1) // virtual page number in TLB, frame number not in TLB, but in RAM
        {
            
            int physical_address = page_table[virtual_page_num]*page_size_Byte + offset;
            update_TLB(virtual_page_num, page_table[virtual_page_num], false, true);
            for(list<Page_tracker>::iterator it = mem_tracker.begin(); it != mem_tracker.end(); ++it)
            {
                if((*it).page_num== page_table[virtual_page_num])
                {
                    (*it).r_bit=1; // set ref to 1
                    break;
                } 
            }
            return physical_address;
        }
        else // virtual page number in TLB, frame number neither in TLB nor in page table (ram)
        {

            return page_fault_handler(virtual_page_num, true, fd)*page_size_Byte + offset;
            //return address_translation(virtual_address, fd);
        }
    }
    else if(result_TLB==-1) // neither virtual page number  and frame number in TLB
    {
        tlb_miss++;
        if(page_table[virtual_page_num]!=-1) //In RAM
        {
            
            int physical_address = page_table[virtual_page_num]*page_size_Byte + offset;
            update_TLB(virtual_page_num, page_table[virtual_page_num], false, false);
            for(list<Page_tracker>::iterator it = mem_tracker.begin(); it != mem_tracker.end(); ++it)
            {
                if((*it).page_num== page_table[virtual_page_num])
                {
                    (*it).r_bit=1; // set ref to 1
                    break;
                } 
            }
            return physical_address;
        }

        return page_fault_handler(virtual_page_num, false, fd)*page_size_Byte + offset;
        //return address_translation(virtual_address, fd);
        //worst case page fault
    }

}


int checkTLB(int virtual_page_num)
{
    
    for(int i=0; i<TLB_capacity; i++)
    {
        if(tlb[i].virtual_page_num==virtual_page_num)
        {
            if(tlb[i].frame_num!=-1)
            {
                return tlb[i].frame_num; // tLB hit
            }
            else return -2; //if virtual page number exits but not frame number, return -2
        }
    }
    return -1; // if both virtual page number and frame number do not exist, return -1
}

int page_fault_handler(int virtual_page_num, bool partial, FILE * fd)
{
    page_fault_counter++;

    memset(buffer, 0, page_size_Byte*sizeof(char));
    if(fd==NULL) printf("Error happens while opening disk_sim file");
    else
    {

       if(virtual_page_num!=-1) // if only  virtual page number exists in TLB, then partial == true; otherwise partial is false.
       {

            fseek(fd, virtual_page_num* page_size_Byte, SEEK_SET);
            int res=fread(buffer, sizeof(char), page_size_Byte, fd);

            if(mem_tracker.size()<num_pages) // TODO: implement moving around Second chance Algo with array in list.
            {


                int empty_frame_pos = mem_tracker.size();
                memcpy(physical_mem[empty_frame_pos].frame, buffer, page_size_Byte);
                Page_tracker page ={ empty_frame_pos, 0};
                mem_tracker.push_back(page); //push array
                page_table[virtual_page_num]=empty_frame_pos; // update page table
                update_TLB(virtual_page_num, empty_frame_pos , false, partial);
                return empty_frame_pos;

            }
            else if(mem_tracker.size()==num_pages)
            {

                for(int i=0; i<mem_tracker.size(); i++)
                {
                    if(mem_tracker.front().r_bit==1 && i<mem_tracker.size()-1)
                    {
                        int page_n= mem_tracker.front().page_num;
                        mem_tracker.pop_front();
                        Page_tracker page ={ page_n, 0};
                        mem_tracker.push_back(page);
                        continue;
                    }
                    else if(mem_tracker.front().r_bit==0)
                    {
                        int page_n= mem_tracker.front().page_num;
                        mem_tracker.pop_front();
                        Page_tracker page ={ page_n, 0};
                        memcpy(physical_mem[page_n].frame, buffer, page_size_Byte);
                        for(int j=0; j<page_table_size; j++) // marking page as removed from Ram
                        {
                            if(page_table[j]==page_n)
                            {
                                page_table[j]=-1;
                                update_TLB(j,-1, true, true); //as removed from ram update TLB
                                break;
                            }
                        }
                        mem_tracker.push_back(page);
                        page_table[virtual_page_num]=page_n; //update page table
                        update_TLB(virtual_page_num, page_n , false, partial);
                        return page_n;
            
                    }
                    else if(i==mem_tracker.size()-1)
                    {
                        int page_n= mem_tracker.front().page_num;
                        mem_tracker.pop_front();
                        Page_tracker page ={ page_n, 0};
                        memcpy(physical_mem[page_n].frame, buffer, page_size_Byte);
                        for(int j=0; j<page_table_size; j++) // marking page as removed from RAM
                        {
                            if(page_table[j]==page_n)
                            {
                                page_table[j]=-1;
                                update_TLB(j,-1, true, true); //as removed from ram update TLB
                                break;
                            }
                        }
                        mem_tracker.push_back(page);
                        page_table[virtual_page_num]=page_n; //update page table
                        update_TLB(virtual_page_num, page_n , false, partial);
                        return page_n;

                    }
                }
            }
       }
    }
}

void update_TLB(int virtual_page_num, int frame_num, bool hit,  bool flag2 )
{

    if(virtual_page_num!=-1 && frame_num!=-1 && hit && !flag2) // TLB hit
    {
        for(int i=0; i<TLB_capacity; i++)
            {
                if(tlb[i].virtual_page_num==virtual_page_num &&  tlb[i].frame_num==frame_num)
                {
                    tlb_track.remove(i);
                    tlb_track.push_back(i);
                    for(list<Page_tracker>::iterator it = mem_tracker.begin(); it != mem_tracker.end(); ++it)
                    {
                        if((*it).page_num== frame_num)
                        {
                            (*it).r_bit=1; // set ref to 1
                            break;
                        }
                    }
                    break;
                }
            }
    }
    else if(virtual_page_num!=-1 && frame_num!=-1 && !hit && flag2) // virtual page number exists in TLB but not frame number
    {
             for(int i=0; i<TLB_capacity; i++)
            {
                if(tlb[i].virtual_page_num==virtual_page_num)
                {
                    tlb[i].frame_num=frame_num;
                    tlb_track.remove(i);
                    tlb_track.push_back(i);

                    break;
                }
            }
    }
    else if(virtual_page_num!=-1 && frame_num!=-1 && !hit && !flag2) // Both virtual page number and frame number does not exist in TLB
    {
        if(tlb_track.size()<TLB_capacity) // if TLB is not full
        {

            if(tlb_track.size()>0)
            {
                int empty_entry_pos=tlb_track.back()+1;
                tlb[empty_entry_pos].virtual_page_num=virtual_page_num;
                tlb[empty_entry_pos].frame_num=frame_num;
                tlb_track.push_back(empty_entry_pos);


            }else if(tlb_track.size()==0)
            {
                tlb[0].virtual_page_num=virtual_page_num;
                tlb[0].frame_num=frame_num;
                tlb_track.push_back(0);

            }

        }
        else if(tlb_track.size()==TLB_capacity) // if TLB is full
        {
            int free_entry_pos=tlb_track.front();
            tlb_track.pop_front();
            tlb[free_entry_pos].virtual_page_num=virtual_page_num;
            tlb[free_entry_pos].frame_num=frame_num;
            tlb_track.push_back(free_entry_pos);

        }
    }
    else if(virtual_page_num!=-1 && frame_num==-1 && hit && flag2) // if page is removed from ram, so update TLB
    {

             for(int i=0; i<TLB_capacity; i++)
            {
                if(tlb[i].virtual_page_num==virtual_page_num)
                {
                    tlb[i].frame_num=frame_num;

                    break;
                }
            }
    }

}

void header(void)
{
     printf("|\t\t  Welcome to MNS Memory Management Unit                  |\n");
    printf("|------------------------------------------------------------------------|\n\n");
    printf("\t\t    ----------------------------------\n");
    printf("\t\t    |       System Information       |\n");
     printf("\t\t    ----------------------------------\n");
    printf("\t\t    | Program address space: %d-bit  |\n\t\t    | Page size: %d bytes           |\n\t\t    | TLB capacity: %d entries       |\n\t\t    | Number of frames: %d           |\n\t\t    | Physical memory size: %d KB    |\n", PAS, page_size_Byte,  TLB_capacity,  num_pages,  (num_pages*page_size_Byte)/1024);
    printf("\t\t    ----------------------------------\n\n");

}

void display_log1(int log_address, int physical_address)
{
     int frame_n= physical_address/page_size_Byte;
     int offset = physical_address%page_size_Byte;
     printf(" ------------------------------------------------------------------------|\n");
     printf(" | Logical address:  %d    |  physical address: %d  |  value:  %d \n\n", log_address, physical_address, physical_mem[frame_n].frame[offset]);
}
void display_log2(void)
{
   
    printf(" ------------------------------------------------------------------------|\n\n");
    printf("\t   || TLB Hit Rate:  %.2f%%, Page Fault Rate:  %.2f%% || \n\t   ----------------------------------------------------\n", ((addr_num-tlb_miss)/addr_num)*100, (page_fault_counter/addr_num)*100);
}
