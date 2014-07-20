
/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  01/02/14 13:29:55
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Ross Meikleham, 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "../core/cpu.h"
#include "../core/memory.h"
#include "../core/disasm.h"
#include "../core/graphics.h"
#include "../core/romInfo.h"
#include "../core/IO.h"
#include "../core/timers.h"
#include "../core/lcd.h"
#include "../core/joypad.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define BREAKPOINT_OFF 0x100009
#define BREAKPOINT_MAX 0xFFFF
#define BREAKPOINT_MIN 0x0

int DEBUG = 0;
long STEP_COUNT = -1;
int BREAKPOINT = BREAKPOINT_OFF;

/* Stores possible frequencies in hz of when the
 * timer should be incremented */
long timer_frequencies[] = {4096, 16384, 65536, 262144};

long timer_frequency;
long clock_speed;


int load_program(const char *filename) {
   
    size_t count;
    uint8_t cur;
    FILE *file;
    char buffer[0x200000]; /*  2MB */
  
    //open file in binary read mode
    //read byte by byte into memory
    if(!(file = fopen(filename,"rb"))) {
        fprintf(stderr, "Error opening file %s\n", filename);
        return 0;
    }
  
    for (count = 0; count < 0x200000; count++) {
    //Read file contents into buffer
        if(!fread(&cur, 1, 1, file)) {
            break;
        }
        buffer[count] = cur;
    }
   
   fclose(file);

   return load_rom(buffer, count);
  
}

/* Performs set of debugging commands  
 * TODO seperate functionality and split
 * into smaller functions */
void get_command() {
    #define BUFSIZE 1024
    int step;
    int breakpoint;
    char buf[BUFSIZE];

    BREAKPOINT = BREAKPOINT_OFF;
    STEP_COUNT = -1;
    
    for(;;) {
   
        printf("->");
        fgets(buf, BUFSIZE, stdin);
        
        if(!strcmp(buf, "exit\n")) {
            exit(EXIT_SUCCESS);
        }
        if(!strcmp(buf, "showregs\n")) {
            print_regs();
        }
        else if (BUFSIZE > 5 && !strncmp(buf, "step ", 5)) {
            if (sscanf(buf+5, "%d", &step) && step > 0) {
                STEP_COUNT = step;
                return;
            } else {
                printf("usage: step [stepcount] where stepcount > 0\n");
            }
        }
        else if (!strcmp(buf, "go\n")) {
            return;
        }
        else if (!strcmp(buf, "help\n") || !strcmp(buf, "h\n")) {
            printf("exit :      exit emulator\n"
                   "showregs:    display current contents of registers\n"
                   "step [n]:    execute n operations\n"
                   "go:          execute forever\n"
                   "setb [n]:    set a breakpoint for address n\n"
                   "showmem [n]: display contents of memory address n\n"
                   "disasm [n]:  dissasemble instruction at address n \n");

        }
        else if (BUFSIZE > 5 && !strncmp(buf,"setb",4)) {
            if(sscanf(buf+5, "%d", &breakpoint) && breakpoint >= BREAKPOINT_MIN && breakpoint <= BREAKPOINT_MAX) {
                BREAKPOINT = breakpoint;
            } else {
                printf("usage: setb [breakpoint] (where breakpoint is between 0x0000 and 0xFFFF inclusive)\n");
            } 
        }
        else if (BUFSIZE > 8 && !strncmp(buf,"showmem",7)) {
            int mem;
            if(sscanf(buf+8, "%x", &mem) == 1 && mem >= 0 && mem <= 0xFFFF) {
                printf("mem for %x:\n",mem);
                for (int y = 0; y < 32; y++) {
                    printf("%d: ",y);
                    for(int x = 0; x < 32; x++) {
                        printf("$%02X ",get_mem((32 * y) + x + mem));
                    }
                    printf("\n");
                }
                //printf("0x%X\n",get_mem(mem));
            } else {
                printf("usage: showmem [address] (where address is between 0x0000 and 0xFFFF inclusive)\n"); 
            }
        }
        else if (BUFSIZE > 7 && !strncmp(buf,"disasm",6)) {
            int mem;
            if(sscanf(buf+7, "%d", &mem) == 1 && mem >= 0 && mem <= 0xFFFF) {
                dasm_instruction(mem, stdout);
                printf("\n");
            } else {
                printf("usage: disasm [address] (where address is between 0x0000 and 0xFFFF inclusive)\n");
            }
        }
        else if (!strcmp(buf,"\n")) {
            ;;
        } 
        else {printf("unknown command\n");}
   }
    
}


int run() {
    long current_cycles;
    int skip_bug = 0;
    long cycles = 0;
    get_command();
    for(;;) { 
        
        if (is_halted() || is_stopped()) {
            current_cycles = 4;
            update_timers(current_cycles);

            if (is_stopped()) {
                key_pressed();
            }
            if (is_halted()) {
                update_graphics(current_cycles);
            }
            
        } else {
            current_cycles = exec_opcode(skip_bug);
            skip_bug = 0;
        } 

        cycles += current_cycles;
        if (cycles > 100) {
            update_keys();
            cycles -= 100;
        }
        skip_bug = check_interrupts();            
        
        if (STEP_COUNT > 0 && --STEP_COUNT == 0) {
            get_command();
        }
   }

    return 0;
}



int main(int argc, char* argv[]) {

    if(argc < 2) {
        printf("usage ./gb game");
        return 1;
    }
   
    if(!load_program(argv[1])){
        printf("failed");
        return 1;
    }
    
    uint16_t i;
    /*  print rom name */
    printf("Game Title:");
    for(i = ROM_NAME_START; i<= ROM_NAME_END; i++) {
        printf("%c",get_mem(i));

    }
   
    printf("\nLicensee: %s", get_licensee());
    printf("\nDestination: %s", get_destination_code(get_mem(DESTINATION_CODE)));
    printf("\nROM size: %dKB",get_rom_size(get_mem(CARTRIDGE_ROM_SIZE)));
    printf("\nRAM save size: %dKB",get_ram_save_size(get_mem(CARTRIDGE_RAM_SIZE)));
    printf("\nCartridge Type: %s",get_cartridge_type());
    printf("\nGameboy Color Only Game:%s", is_colour_compatible() ? "Yes":"No");
    printf("\nSuper Gameboy Features:%s", has_sgb_features() ? "Yes":"No");
    printf("\n");
    
    init_gfx();
    init_joypad();
    reset_cpu();
    run();

}

