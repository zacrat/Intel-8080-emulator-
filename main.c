#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAG_S  0x80
#define FLAG_Z  0x40
#define FLAG_AC 0x10
#define FLAG_P  0x04
#define FLAG_CY 0x01

uint8_t memory[65536];

static const uint8_t cycle_table[256] = {
    //  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
    4, 10,  7,  5,  5,  5,  7,  4,  4, 10,  7,  5,  5,  5,  7,  4, // 0x00
    4, 10,  7,  5,  5,  5,  7,  4,  4, 10,  7,  5,  5,  5,  7,  4, // 0x10
    4, 10, 16,  5,  5,  5,  7,  4,  4, 10, 16,  5,  5,  5,  7,  4, // 0x20
    4, 10, 13,  5, 10, 10, 10,  4,  4, 10, 13,  5,  5,  5,  7,  4, // 0x30
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5, // 0x40
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5, // 0x50
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5, // 0x60
    7,  7,  7,  7,  7,  7,  7,  7,  5,  5,  5,  5,  5,  5,  7,  5, // 0x70 (0x76 HLT)
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, // 0x80
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, // 0x90
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, // 0xA0
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, // 0xB0
    5, 10, 10, 10, 11, 11,  7, 11,  5, 10, 10, 10, 11, 17,  7, 11, // 0xC0
    5, 10, 10, 10, 11, 11,  7, 11,  5, 10, 10, 10, 11, 17,  7, 11, // 0xD0
    5, 10, 10, 18, 11, 11,  7, 11,  5,  5, 10,  5, 11, 17,  7, 11, // 0xE0
    5, 10, 10,  4, 11, 11,  7, 11,  5,  5, 10,  4, 11, 17,  7, 11  // 0xF0
};

int cycle_counter = 0;
int extra_cycles = 0;

//Registers
uint8_t A;
uint8_t B;
uint8_t C;
uint8_t D;
uint8_t E;
uint8_t H;
uint8_t L;
uint8_t F;

uint16_t SP;
uint16_t PC;

uint8_t flags = 0x02;

int int_enable = 0;

// IO vars

// Unhandled
uint8_t port_in(uint8_t port) {
    (void)port;
    return 0;
}

void port_out(uint8_t port, uint8_t value) {
    (void)port;
    (void)value;
}


int load_program(const char *filename, uint16_t load_address){
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0 || size > (long)(sizeof(memory) - load_address)) {
        fprintf(stderr, "File size is invalid\n");
        fclose(f);
        return 0;
    }

    size_t bytes_read = fread(memory + load_address, 1, size, f);
    PC = load_address;
    fclose(f);

    if (bytes_read != (size_t)size) {
        fprintf(stderr, "File read error\n");
        return 0;
    }

    printf("Loaded program\n");
    return 1;
}

uint8_t get_reg(int num) {
    switch (num) {
        case 0: {return B;} case 1: {return C;}
        case 2: {return D;} case 3: {return E;}
        case 4: {return H;} case 5: {return L;}
        case 6: {return memory[(H<<8)|L];} default: {return A;}
    }
}

int set_reg(int num, uint8_t val) {
    switch (num) {
        case 0: {B = val; return 1;} case 1: {C = val; return 1;}
        case 2: {D = val; return 1;} case 3: {E = val; return 1;}
        case 4: {H = val; return 1;} case 5: {L = val; return 1;}
        case 6: {memory[(H<<8)|L]= val; return 1;}
        default: {A = val; return 1;}
    }
}

uint16_t get_rp(int rp) {
    switch (rp) {
        case 0: return (B << 8) | C;
        case 1: return (D << 8) | E;
        case 2: return (H << 8) | L;
        default: return SP;
    }
}

void set_rp(int rp, uint16_t val) {
    switch (rp) {
        case 0: B = val >> 8; C = val & 0xFF; break;
        case 1: D = val >> 8; E = val & 0xFF; break;
        case 2: H = val >> 8; L = val & 0xFF; break;
        default: SP = val; break;
    }
}

uint16_t fetch16(void) {
    uint16_t v = memory[PC] | (memory[(uint16_t)(PC + 1)] << 8);
    PC += 2;
    return v;
}

void set_szp(uint8_t v) {
    flags &= ~(FLAG_S | FLAG_Z | FLAG_P);
    if (v & FLAG_S) flags |= FLAG_S;
    if (v == 0) flags |= FLAG_Z;
    int bits = 0;
    for (int i = 0; i < 8; i++) bits += (v>>i) & 1;
    if ((bits&1)==0) flags |= FLAG_P;
}

int cond(int c) {
    switch (c) {
        case 0: return !(flags&FLAG_Z);
        case 1: return (flags&FLAG_Z);
        case 2: return !(flags&FLAG_CY);
        case 3: return (flags&FLAG_CY);
        case 4: return !(flags&FLAG_P);
        case 5: return (flags&FLAG_P);
        case 6: return !(flags&FLAG_S);
        default: return (flags&FLAG_S);
    }
}

int get_rp_psw(int rp) {
    if (rp == 3) return (A<<8) | flags;
    return get_rp(rp);
}

void set_rp_psw(int rp, int val) {
    if (rp == 3) {
        A = val>>8;
        flags = (val & 0xD5)|0x02;
    }
    else {
        set_rp(rp, val);
    }
}

void push(uint16_t val) {
    memory[--SP] = val>>8;
    memory[--SP] = val & 0xFF;
}

uint16_t pop(void) {
    uint8_t lo = memory[SP++];
    uint8_t hi = memory[SP++];
    return lo | (hi << 8);
}

void alu(int op, uint8_t val) {
    int cy = (flags & FLAG_CY) ? 1:0;
    switch (op) {
        case 0:case 1: {
            int c = (op == 1) ? cy : 0;
            int res = A + val + c;
            flags &= ~(FLAG_AC | FLAG_CY);
            if (((A & 0x0F) + (val & 0x0F) + c) > 0x0F) flags |= FLAG_AC;
            if (res > 0xFF) flags |= FLAG_CY;
            A = res;
            set_szp(res);
            break;
        }
        case 2: case 3: case 7: {
            int c = (op == 3) ? cy : 0;
            int res = A - val - c;
            flags &= ~(FLAG_AC | FLAG_CY);
            if (((A & 0x0F) + (~val & 0x0F) + !c) > 0x0F) flags |= FLAG_AC;
            if (res < 0) flags |= FLAG_CY;
            set_szp(res);
            if (op != 7) A =res;
            break;
        }
        case 4: {
            flags &= ~(FLAG_AC | FLAG_CY);
            if ((A | val) & 0x08) flags |= FLAG_AC;
            A &= val;
            set_szp(A);
            break;
        }
        case 5: {
            A ^= val;
            flags &= ~(FLAG_AC | FLAG_CY);
            set_szp(A);
            break;
        }
        default: {
            A |= val;
            flags &= ~(FLAG_AC | FLAG_CY);
            set_szp(A);
            break;
        }
    }
}

// Hardware interrupt: behaves like an RST n instruction pushed onto the bus.
// n = 1 -> vector 0x08, n = 2 -> vector 0x10. Interrupts are disabled until the
// program executes EI again (the game's handler ends with EI + RET).
void interrupt(int n) {
    int_enable = 0;
    push(PC);
    PC = n << 3;
}

int step() {
    // Handles print out
    if (PC == 0x0005) {
        if (C == 2) {
            putchar(E);
        } else if (C == 9) {
            uint16_t addr = get_rp(1);
            while (memory[addr] != '$') putchar(memory[addr++]);
        }
        fflush(stdout);
        PC = pop();
    }

    uint8_t instruction = memory[PC++];
    extra_cycles = 0;
    uint8_t block = (instruction >> 6) & 3;
    uint8_t ddd = (instruction >> 3) & 7;
    uint8_t sss = instruction & 7;


    switch (block) {
        case 0: {
            switch (sss) {
                case 0: {break;}
                case 1: {
                    switch (ddd & 1) {
                        case 0: {   // LXI
                            set_rp(ddd >> 1, fetch16());
                            break;
                        }
                        case 1: {
                            uint32_t res = get_rp(2) + get_rp(ddd>>1);
                            set_rp(2, res);
                            flags &= ~FLAG_CY;
                            if (res > 0xFFFF) flags |= FLAG_CY;
                            break;
                        }
                        default: {break;}
                    }
                    break;
                }
                case 2: {
                    switch (ddd) {
                        case 0: {
                            memory[B<<8|C] = A;
                            break;
                        }
                        case 1: {
                            A = memory[B<<8|C];
                            break;
                        }
                        case 2: {
                            memory[D<<8|E] = A;
                            break;
                        }
                        case 3: {
                            A = memory[D<<8|E];
                            break;
                        }
                        case 4: {
                            uint16_t addr = fetch16();
                            memory[addr] = L;
                            memory[(uint16_t)(addr + 1)] = H;
                            break;
                        }
                        case 5: {
                            uint16_t addr = fetch16();
                            L = memory[addr];
                            H = memory[(uint16_t)(addr + 1)];
                            break;
                        }
                        case 6: {
                            uint16_t addr = fetch16();
                            memory[addr] = A;
                            break;
                        }
                        case 7: {
                            uint16_t addr = fetch16();
                            A = memory[addr];
                            break;
                        }
                    }
                    break;
                }
                case 3: {
                    uint8_t pair = ddd>>1;
                    switch (ddd & 1) {
                        case 0: {   // INX
                            set_rp(pair, get_rp(pair) + 1);
                            break;
                        }
                        case 1: {   // DNX
                            set_rp(pair, get_rp(pair) - 1);
                            break;
                        }
                    }
                    break;
                }
                case 4: { //INR
                    uint8_t res = get_reg(ddd) + 1;
                    set_reg(ddd, res);
                    set_szp(res);
                    flags &= ~FLAG_AC;
                    if ((res&0x0F) == 0x00) flags |= FLAG_AC;
                    break;
                }
                case 5: { //DCR
                    uint8_t res =  get_reg(ddd) - 1;
                    set_reg(ddd, res);
                    set_szp(res);
                    flags &= ~FLAG_AC;
                    if ((res&0x0F) != 0x0F) flags |= FLAG_AC;
                    break;
                }
                case 6: { //MOV
                    set_reg(ddd, memory[PC++]);
                    break;
                }

                case 7: {
                    switch (ddd) {
                        case 0: {   // RLC
                            uint8_t out = A >> 7;
                            A = (A << 1) | out;
                            flags = (flags & ~FLAG_CY) | out;
                            break;
                        }
                        case 1: {   // RRC
                            uint8_t out = A & 1;
                            A = (A >> 1) | (out << 7);
                            flags = (flags & ~FLAG_CY) | out;
                            break;
                        }
                        case 2: {   // RAL
                            uint8_t out = A >> 7;
                            A = (A << 1) | (flags & FLAG_CY);       // old CY → bit 0
                            flags = (flags & ~FLAG_CY) | out;
                            break;
                        }
                        case 3: {   // RAR
                            uint8_t out = A & 1;
                            A = (A >> 1) | ((flags & FLAG_CY) << 7); // old CY → bit 7
                            flags = (flags & ~FLAG_CY) | out;
                            break;
                        }
                        case 4: {
                            uint8_t lo = A & 0x0F;
                            uint8_t hi = A >> 4;
                            uint8_t correction = 0;
                            int cy = (flags & FLAG_CY) ? 1 : 0;

                            if ((flags & FLAG_AC) || lo > 9) correction |= 0x06;
                            if (cy || hi > 9 || (hi >= 9 && lo > 9)) { correction |= 0x60; cy = 1; }

                            flags &= ~(FLAG_AC | FLAG_CY);
                            if ((lo + (correction & 0x0F)) > 0x0F) flags |= FLAG_AC;
                            A += correction;
                            set_szp(A);
                            if (cy) flags |= FLAG_CY;
                            break;
                        }
                        case 5: { // CMA
                            A = ~A;
                            break;
                        }
                        case 6: {
                            flags |= FLAG_CY;
                            break;
                        }
                        case 7: {
                            flags ^= FLAG_CY;
                            break;
                        }
                    }
                    break;
                }

            }
            break;
        }
        case 1: { // MOV
            if (instruction == 0x76) {return 0; break;}
            set_reg(ddd, get_reg(sss));
            break;
        }

        case 2: {alu(ddd, get_reg(sss)); break;} // Arithemetic operations


        case 3:{
            switch (sss) {
                case 0: {if (cond(ddd)) {PC = pop(); extra_cycles = 6;} break;}
                case 1: {
                    switch (ddd%2) {
                        case 0: {
                            set_rp_psw(ddd >> 1, pop()); // POP rp
                            break;
                        }
                        case 1: {
                            switch (ddd) {
                                case 1: case 3: PC = pop(); break; // RET + alias
                                case 5: PC = get_rp(2); break; //PCHL
                                case 7: SP = get_rp(2); break; // SPHL
                            }
                            break;
                        }
                    }
                    break;
                }

                case 2: { // Conditional JMP
                    uint16_t addr = fetch16();
                    if (cond(ddd)) {PC = addr;}
                    break;
                }
                case 3: {
                    switch (ddd) {
                        case 0: {PC = fetch16(); break;} // JMP
                        case 1: {PC = fetch16();break;}//Illegal JMP
                        case 2: {port_out(memory[PC++], A);break;}//OUT
                        case 3: {A = port_in(memory[PC++]);break;}//IN
                        case 4: {//XTHL
                            uint16_t temp = pop();
                            push(get_rp(2));
                            set_rp(2, temp);
                            break;
                        }
                        case 5: {
                            uint16_t temp = get_rp(1);
                            set_rp(1, get_rp(2));
                            set_rp(2, temp);
                            break;
                        }//XCHG
                        case 6: {int_enable = 0;break;}//DI
                        case 7: {int_enable = 1;break;}//EI
                    }
                    break;
                }

                case 4: { //Conditional calls
                    uint16_t addr = fetch16();
                    if (cond(ddd)) {push(PC);PC = addr; extra_cycles = 6;}
                    break;
                }

                case 5: {
                    if ((ddd&1)==0) {
                        push(get_rp_psw(ddd >>1)); // PUSH rp
                    }
                    else {
                        uint16_t addr = fetch16(); //CALL a16
                        push(PC);
                        PC = addr;
                    }
                    break;
                }
                case 6: {alu(ddd, memory[PC++]); break;}
                case 7: { // RST
                    push(PC);
                    PC = ddd<<3;
                    break;
                }

            }
            break;
        }

        default: {
            printf("Unhandled block %02x\n", instruction);
        }

    }

    cycle_counter += cycle_table[instruction] + extra_cycles;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s program.com [load_address_hex]\n", argv[0]);
        return 1;
    }
    uint16_t load_addr = (argc >= 3) ? (uint16_t)strtoul(argv[2], NULL, 16) : 0x0100;

    memory[0] = 0x76;
    if (!load_program(argv[1], load_addr)) return 1;

    while (step()) {}

    printf("\nHalted at PC=%04X after %d cycles\n", PC, cycle_counter);
    return 0;
}
