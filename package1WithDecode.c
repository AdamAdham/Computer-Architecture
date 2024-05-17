#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

uint32_t *memoryUnit;
uint32_t PC = 0; // could be 11 bits
uint32_t R[32]; //array of your 32 registers (PC is the final reg.)
uint32_t instructions[4];   //bec. we hav 4 instructions that can be done in parallel in same clock cycle --> pipline in project description
uint16_t instructionsStage[4] = {0};  //bec. each instruction of the 4 could have a different stage (fetch - decode - execute - writeback)
                                      // recall: the fetch (IF) and memory (MEM) stages cannot be done in parallel since they access same physical memory
bool instructionActive[4] = {false};
int RegisterDestination[32] = {-1}; // 

char* getStringOpcode(int opcode,char* returnValue) {
    switch(opcode) {
        case 0: strcpy(returnValue,"ADD"); break;
        case 1: strcpy(returnValue,"SUB"); break;
        case 2: strcpy(returnValue,"MULI"); break;
        case 3: strcpy(returnValue,"ADDI"); break;
        case 4: strcpy(returnValue,"BNE"); break;
        case 5: strcpy(returnValue,"ANDI"); break;
        case 6: strcpy(returnValue,"ORI"); break;
        case 7: strcpy(returnValue,"J"); break;
        case 8: strcpy(returnValue,"SLL"); break;
        case 9: strcpy(returnValue,"SRL"); break;
        case 10: strcpy(returnValue,"LW"); break;
        case 11: strcpy(returnValue,"SW"); break;

        default: return "UNKNOWN";
    }
}

char* instructionToString(int instruction, char* buffer, size_t buffer_size){
        int opcode = (instruction & 0b11110000000000000000000000000000)>>28; 
        char opcodeString[5];
        getStringOpcode(opcode,opcodeString);
        int R1Address = (instruction & 0b00001111100000000000000000000000)>>23;  
        int R2Address =  (instruction & 0b00000000011111000000000000000000)>>18;   
        int R3Address = (instruction & 0b00000000000000111110000000000000)>>13; 
        int shamt = (instruction & 0b00000000000000000001111111111111);      
        int imm = (instruction & 0b00000000000000111111111111111111)>>0; 
        int MSB = imm & 0b100000000000000000;
        MSB = MSB >> 17;
        if(MSB==1){
            // Negative number
            imm = imm ^ 0b111111111111111111;
            imm = imm + 1;
            imm = -imm;
        }  
          
        int address = (instruction & 0b00001111111111111111111111111111)>>0;
    switch(opcode) {
        case 0:
        case 1: snprintf(buffer, buffer_size,"%s R%d R%d R%d",opcodeString,R1Address,R2Address,R3Address); break;
        case 2: 
        case 3:
        case 4: 
        case 5: 
        case 6: 
        case 10: // Getting all that use R1 R2 imm
        case 11: snprintf(buffer, buffer_size,"%s R%d R%d %d",opcodeString,R1Address,R2Address,imm); break;
        case 7: snprintf(buffer, buffer_size,"%s %d",opcodeString,address); break;
        case 8: 
        case 9: snprintf(buffer, buffer_size,"%s R%d R%d %d",opcodeString,R1Address,R2Address,shamt); break;
        default: "";break;
    }
}

struct instructionData{
    int opcode;
    int R1;  //r
    int R2;  //r 
    int R3;  //r
    int R1Address;  //r
    int R2Address;  //r 
    int R3Address;  //r
    int shamt;
    int funct;   
    signed int imm;   
    int address;
    int loadStoreAddress; 
    int instructionAddress; // Address of the instruction itself  
    int aluResult;
    bool branch;
    int dependantIndex;
    int clockCycleEntered;
    int memoryValue;
};
struct instructionData instructionDataArray[4]; 

void flushInstructions(int branchInstructionIndex){
    int branchClockCycleEntered = instructionDataArray[branchInstructionIndex].clockCycleEntered;
    // printf("Branch address: %d\n",instructionDataArray[branchInstructionIndex].instructionAddress);
    // printf("CLcck cycle of branch: %d\n",branchClockCycleEntered);
    for(int i=0;i<4;i++){
        int clockCycleEntered =  instructionDataArray[i].clockCycleEntered;
        // printf("CLock cycle entered: %d\n",clockCycleEntered);
        if(i!=branchInstructionIndex && clockCycleEntered>branchClockCycleEntered && clockCycleEntered!=branchClockCycleEntered+6){
            // 1st condition: Flush the instructions that is not the branch one
            // &&
            // 2nd condition: Flush the instructions that came after the branch not before
            // printf("inst stage: %d\n",instructionsStage[i]);
            char string[50];
            instructionToString(instructions[i], string, sizeof(string));
            printf("FLush:  %s\n",string);
            instructions[i] = -1;
            instructionsStage[i] = 0; 
            instructionActive[i] = false;
        }  
    }
    printf("\n");
}

// boolean array so that no 2 threads can do on same instr
// array of srtuct to save instr data
// TODO See register type

int getSizeOfTextFile() {
    FILE *file;
    char filename[] = "instrTest.txt";
    int line_count = 0;
    char ch;

    file = fopen(filename, "r");

    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            line_count++;
        }
    }

    fclose(file);
    return line_count;
}

typedef enum {
    ADD,
    SUB,
    SLL,
    SRL,
    MULI,
    ADDI,
    BNE,
    ANDI,
    ORI,
    LW,
    SW,
    J
} Opcode;

Opcode getOpcode(const char *opcode_str) {
    if (strcmp(opcode_str, "ADD") == 0) {
        return ADD;
    } else if (strcmp(opcode_str, "SUB") == 0) {
        return SUB;
    } else if (strcmp(opcode_str, "SLL") == 0) {
        return SLL;
    } else if (strcmp(opcode_str, "SRL") == 0) {
        return SRL;
    } else if (strcmp(opcode_str, "MULI") == 0) {
        return MULI;
    } else if (strcmp(opcode_str, "ADDI") == 0) {
        return ADDI;
    } else if (strcmp(opcode_str, "BNE") == 0) {
        return BNE;
    } else if (strcmp(opcode_str, "ANDI") == 0) {
        return ANDI;
    } else if (strcmp(opcode_str, "ORI") == 0) {
        return ORI;
    } else if (strcmp(opcode_str, "LW") == 0) {
        return LW;
    } else if (strcmp(opcode_str, "SW") == 0) {
        return SW;
    } else if (strcmp(opcode_str, "J") == 0) {
        return J;
    } else {
        return -1; // unknown opcode
    }
}

void readInstructions() {
    FILE *file;
    char filename[] = "instrTest.txt";
    char line[1000]; // Adjust the size according to your needs
    char *token;
    char *ptr;
    int reg_num;

    file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    int counter = 0;
    while (fgets(line, sizeof(line), file)) {
        token = strtok(line, " ");
        Opcode opcodeO = getOpcode(token);
        uint32_t instruction;
        if (token != NULL) {
            bool isR = false;
            bool isI = false;
            bool isJ = false;
            int opcode;
            switch(opcodeO) {
                case ADD: isR = true; opcode = 0b0000; break;
                case SUB: isR = true; opcode = 0b0001; break;
                case SLL: isR = true; opcode = 0b1000; break;
                case SRL: isR = true; opcode = 0b1001; break;
                case MULI: isI = true; opcode = 0b0010; break;
                case ADDI: isI = true; opcode = 0b0011; break;
                case BNE: isI = true; opcode = 0b0100; break;
                case ANDI: isI = true; opcode = 0b0101; break;
                case ORI: isI = true; opcode = 0b0110; break;
                case LW: isI = true; opcode = 0b1010; break;
                case SW: isI = true; opcode = 0b1011; break;
                case J: isJ = true; opcode = 0b0111; break;
            }
            instruction = (opcode << 28);
            if(isR) {
                token = strtok(NULL, " - ");
                reg_num = strtol(token + 1, &ptr, 10);
                printf("Binary value of R%d: %d\n", reg_num, reg_num);
                instruction += (reg_num << 23);

                token = strtok(NULL, " - ");
                reg_num = strtol(token + 1, &ptr, 10);
                printf("Binary value of R%d: %d\n", reg_num, reg_num);
                instruction += (reg_num << 18);

                token = strtok(NULL, " - ");
                if (strncmp(token, "R", 1) == 0) {
                    reg_num = strtol(token + 1, &ptr, 10);
                    printf("Binary value of R%d: %d\n", reg_num, reg_num);
                    instruction += (reg_num << 13);
                } else {
                    reg_num = strtol(token, &ptr, 10);
                    printf("value of shamt is %d: %d\n", reg_num, reg_num);
                    instruction += reg_num;
                }
            } else if(isI) {
                token = strtok(NULL, " - ");
                reg_num = strtol(token + 1, &ptr, 10);
                printf("Binary value of R%d: %d\n", reg_num, reg_num);
                instruction += (reg_num << 23);

                token = strtok(NULL, " - ");
                reg_num = strtol(token + 1, &ptr, 10);
                printf("Binary value of R%d: %d\n", reg_num, reg_num);
                instruction += (reg_num << 18);
                token = strtok(NULL, " ");
                reg_num = strtol(token, &ptr, 10);
                signed int imm = atoi(token);
                printf("imm value in read: %d\n\n\n",imm);
                printf("instruction before +imm: %d\n",instruction);
                if(imm<0){
                    imm = imm ^ 0b111111111111111111;
                    imm = -(imm + 1);
                    printf("imm: 2's comp:%d\n",imm);
                    instruction+=imm;
                }else{
                    instruction+=imm;
                }
                printf("instruction after +imm: %d\n",instruction);
            } else if(isJ) {
                token = strtok(NULL, " - ");
                reg_num = strtol(token, &ptr, 10);
                printf("value of address is %d: %d\n", reg_num, reg_num);
                instruction += reg_num;
            } else {
                printf("problemo");
            }
        }
        printf("%d was added \n\n",instruction);
        *(memoryUnit+counter) = instruction;
        if(counter == 1023) {
            break;
        }
        counter++;
    }   
    fclose(file);
}

bool isLW(char *str) {
    // Extract the first 4 characters of the string
    char first_2_chars[2];  // Make room for the null terminator
    strncpy(first_2_chars, str, 2);

    if(strcmp(first_2_chars,"LW")==0){
        return true;
    }    
    return false;  // No match found
}

bool notR1Instruction(char *str) {
    // Extract the first 4 characters of the string
    char first_3_chars[3];  // Make room for the null terminator
    strncpy(first_3_chars, str, 3);

    char first_2_chars[2];  // Make room for the null terminator
    strncpy(first_2_chars, str, 2);

    char firstChar = str[0];

    if(strcmp(first_3_chars,"BNE")==0 || firstChar == 'J' || strcmp(first_2_chars,"SW")==0){
        return true;
    }    
    return false;  // No match found
}

uint32_t concatenate_bits(uint32_t PC, uint32_t imm) {
    // TODO TEST
    // Extract the desired bits from PC and imm
    uint32_t PC_bits = PC & 0xF0000000;  // PC[31:28]
    uint32_t imm_bits = imm & 0x0FFFFFFF;   // imm[27:0]

    // Concatenate the extracted bits
    uint32_t concatenated_value = PC_bits | imm_bits;

    return concatenated_value;
}

bool isPCInstruction(char *str) {
    // Extract the first 4 characters of the string
    char first_3_chars[3];  // Make room for the null terminator
    strncpy(first_3_chars, str, 3);
    char firstChar = str[0];

    if(strcmp(first_3_chars,"BNE")==0 || firstChar == 'J'){
        return true;
    }    
    return false;  // No match found
}

bool isFirst4CharactersInArray(char *str, char *array[], int array_size) {
    // Extract the first 4 characters of the string
    char first_4_chars[5];  // Make room for the null terminator
    strncpy(first_4_chars, str, 4);
    first_4_chars[4] = '\0';  // Null terminate the string

    // Iterate over the array of strings
    for (int i = 0; i < array_size; i++) {
        // Compare the first 4 characters with each string in the array
        if (strcmp(first_4_chars, array[i]) == 0) {
            return true;  // Match found
        }
    }

    return false;  // No match found
}

void fetch(int clockCycle) {
    // In description says that we fetch from memory
    if(PC<1024 && memoryUnit[PC]!=0){
        int i=0;
        while(instructionsStage[i]!=0 && i<4){
            i++;
        }
        if(i>=4) return;
        instructionDataArray[i].clockCycleEntered = clockCycle;
        instructions[i] = memoryUnit[PC];
        instructionDataArray[i].instructionAddress = PC;
        PC = PC + 1;
        instructionsStage[i] = 1;
        instructionActive[i] = true;
        char string[50];
        instructionToString(instructions[i], string, sizeof(string));
        
        printf("Instruction:  %s     |      Stage: Fetch \n",string);
    }
}

void decode(int32_t instructionIndex){
    // get instr and do the same as the task
        int instruction = instructions[instructionIndex];

        instructionDataArray[instructionIndex].opcode = (instruction & 0b11110000000000000000000000000000)>>28;  
        instructionDataArray[instructionIndex].R1Address = (instruction & 0b00001111100000000000000000000000)>>23;  
        instructionDataArray[instructionIndex].R1 = R[instructionDataArray[instructionIndex].R1Address];
        instructionDataArray[instructionIndex].R2Address =  (instruction & 0b00000000011111000000000000000000)>>18;   
        instructionDataArray[instructionIndex].R2 = R[instructionDataArray[instructionIndex].R2Address];
        instructionDataArray[instructionIndex].R3Address = (instruction & 0b00000000000000111110000000000000)>>13; 
        instructionDataArray[instructionIndex].R3 = R[instructionDataArray[instructionIndex].R3Address];
        instructionDataArray[instructionIndex].shamt = (instruction & 0b00000000000000000001111111111111);      
        instructionDataArray[instructionIndex].imm = (instruction & 0b00000000000000111111111111111111)>>0; 
        instructionDataArray[instructionIndex].address = (instruction & 0b00001111111111111111111111111111)>>0;

        // printf("\n \n\n\n\n\n\n\n\n");

        // printf("Instruction: %d\n",instruction);
        // printf("opcode: %d \n", instructionDataArray[instructionIndex].opcode);
        // printf("R1: %d \n", R[instructionDataArray[instructionIndex].R1]);
        // printf("R1Address: %d \n", instructionDataArray[instructionIndex].R1Address);
        // printf("R2Address: %d \n", instructionDataArray[instructionIndex].R2Address);
        // printf("R2: %d \n", R[instructionDataArray[instructionIndex].R2]);
        // printf("R3: %d \n", R[instructionDataArray[instructionIndex].R3]);
        // printf("shamt: %d \n", instructionDataArray[instructionIndex].shamt);
        // printf("imm: %d \n", instructionDataArray[instructionIndex].imm);
        // printf("address: %d \n", instructionDataArray[instructionIndex].address);

        instructionsStage[instructionIndex]+=1;
}

void execute(int32_t instructionIndex){
    // add all params rs,R3,rt etc and acc to opcode we use the relevant arguement
    // immediate signed
    int opcode = instructionDataArray[instructionIndex].opcode; //.opcode is from the structure
    int R1 = instructionDataArray[instructionIndex].R1;
    int R1Address = instructionDataArray[instructionIndex].R1Address;
    int R2 = instructionDataArray[instructionIndex].R2;
    int R3 = instructionDataArray[instructionIndex].R3;
    int shamt = instructionDataArray[instructionIndex].shamt;
    signed int imm = instructionDataArray[instructionIndex].imm;

    // Handling Negative values
    bool negativeFlag = false;
    int MSB = imm & 0b100000000000000000;
    MSB = MSB >> 17;
    if(MSB==1){
        // Negative number
        negativeFlag = true;
        imm = imm ^ 0b111111111111111111;
        imm = imm + 1;
    }
    int res;
    int address = instructionDataArray[instructionIndex].address;
    switch (opcode)
    {
        case 0:
            // ADD
            instructionDataArray[instructionIndex].aluResult = R2 + R3;
            break;
        case 1:
            // SUB
            instructionDataArray[instructionIndex].aluResult = R2 - R3;
            break;
        case 2:
            // MULI
            if(negativeFlag){
                instructionDataArray[instructionIndex].aluResult = R2 * (-imm);   
            }else{
                instructionDataArray[instructionIndex].aluResult = R2 * imm; 
            }    
            break;

        case 3:
            // ADDI
            if(negativeFlag){
                instructionDataArray[instructionIndex].aluResult = R2 + (-imm);   
            }else{
                instructionDataArray[instructionIndex].aluResult = R2 + imm; 
            }     
            break;

        case 4:
            // BNE
            res = R1-R2;
            printf("PC was %i in Execute Stage \n",PC);
            if(res!=0){
                if(negativeFlag){
                    PC = PC + (-imm);  //pc + 1 is already done so we'll add the imm directly --> check the remarks document
                }else{
                    PC = PC + imm;
                }
                printf("PC was changed to %i in Execute Stage \n",PC);
                instructionDataArray[instructionIndex].branch = true;
            }else{
                printf("PC was unchanged \n");
            }          
            break;

        case 5:
            // ANDI
            if(negativeFlag){   
                instructionDataArray[instructionIndex].aluResult = R2 & (-imm);  
            }else{
                instructionDataArray[instructionIndex].aluResult = R2 & imm;  
            }      
            break;

        case 6:
            // ORI
            if(negativeFlag){
                instructionDataArray[instructionIndex].aluResult = R2 | (-imm);
            }else{
                instructionDataArray[instructionIndex].aluResult = R2 | imm;
            }
            break;

        case 7:
            // J
            instructionDataArray[instructionIndex].branch = true;
            printf("PC was %i in Execute Stage \n",PC);
            PC = concatenate_bits(PC,address);
            printf("PC was changed to %i in Execute Stage \n",PC);
            break;

        case 8:
            // SLL
            instructionDataArray[instructionIndex].aluResult = R2 << shamt;
            break;

        case 9:
            // SRL
            instructionDataArray[instructionIndex].aluResult = R[R1Address] = R2 >> shamt;  
            break;

        case 10:
            // LW            
            // R1 = memoryUnit[R2+imm];     --> this accesses the memory which is incorrect in execute stage
            //loadStoreAddress holds R2+imm (ALU operation)
            if(negativeFlag){
                instructionDataArray[instructionIndex].loadStoreAddress = R2 + (-imm);
            }else{
                instructionDataArray[instructionIndex].loadStoreAddress = R2 + imm;
            } 
            break;

        case 11:
            // SW
            if(negativeFlag){
                instructionDataArray[instructionIndex].loadStoreAddress = R2 + (-imm);
            }else{
                instructionDataArray[instructionIndex].loadStoreAddress = R2 + imm;
            }
            printf("SW instruction \n");
            break;

    default:
        printf("OPERATION NOT SUPPORTED \n");
        break;
    }
    
    instructionsStage[instructionIndex]+=1;
}

void memory(int32_t instructionIndex){
    int opcode = instructionDataArray[instructionIndex].opcode;
    int memoryLocation = instructionDataArray[instructionIndex].loadStoreAddress;
    int R1 = instructionDataArray[instructionIndex].R1;
    int R1Address = instructionDataArray[instructionIndex].R1Address;
    if(opcode==10){
        // LW
        // printf("R%i was %i in Memory stage \n",R1Address,R1);
        if(memoryLocation<0){
            printf("Memory address location %d is invalid at Memory Stage\n",memoryLocation);
        }else{
            instructionDataArray[instructionIndex].memoryValue =  memoryUnit[memoryLocation];
        }
        // printf("R%i has changed to %i in Memory stage \n",R1Address,R[R1Address]);
    }else if(opcode==11){
        // SW
        // since only memory is LW and SW
        printf("Memory address location %i was %i in Memory stage \n",memoryLocation,memoryUnit[memoryLocation]);
        if(memoryLocation<1023){
            printf("Memory address location %i was not changed (due to invalid memory address) \n",memoryLocation);
        }else{
            memoryUnit[memoryLocation] = R1;
            printf("Memory address location %i has changed to %i in Memory stage \n",memoryLocation,memoryUnit[memoryLocation]);
        }
    }
    instructionsStage[instructionIndex] += 1;
}

bool writeback(int32_t instructionIndex){
    char *generalRegisterOpcodes[] = {"ADD ", "SUB ", "MULI", "ADDI", "ANDI", "ORI ", "SLL ","SRL "};
    char string[50];
    instructionToString(instructions[instructionIndex], string, sizeof(string));
    printf("Instruction:  %s     |      Stage: Writeback \n",string);
    int R1Address = instructionDataArray[instructionIndex].R1Address;
    if(isFirst4CharactersInArray(string, generalRegisterOpcodes, sizeof(generalRegisterOpcodes) / sizeof(generalRegisterOpcodes[0]))) {     
        printf("R%i was %i in Write Back stage \n",R1Address,R[R1Address]);
        if(R1Address==0){
            printf("R0 has not been changed \n\n");
        }else{       
            R[R1Address] = instructionDataArray[instructionIndex].aluResult;
            printf("R%i has changed to %i in Write Back stage \n\n",R1Address,R[R1Address]);
        }
    //} else if(isPCInstruction(string)){
    //     printf("PC was %i in Write Back stage \n",PC);
    //     if(instructionDataArray[instructionIndex].branch){
    //         PC = instructionDataArray[instructionIndex].aluResult + 1;
    //         printf("PC was changed to %i in Write Back stage \n",PC);
    //     }else{
    //         printf("PC was unchanged \n");
    //     }      
    }else if(isLW(string)){
        int R1 = instructionDataArray[instructionIndex].R1;
        int memoryLocation = instructionDataArray[instructionIndex].loadStoreAddress;
        if(memoryLocation<0){
            printf("Memory address location %d is invalid at Write Back Stage\n",memoryLocation);
        }else{
            printf("R%i was %i in Write Back stage \n",R1Address,R1);
            R[R1Address] =  instructionDataArray[instructionIndex].memoryValue;
            printf("R%i has changed to %i in Write Back stage \n",R1Address,R[R1Address]);
        }
    }
    // Reset instruction data
    bool flushed = false;
    if(instructionDataArray[instructionIndex].branch){
        // If a J or BNE instruction
        // &&
        // branch will occur (BNE condition is true || J instruction)
        flushInstructions(instructionIndex);
        flushed = true;
    }
    instructions[instructionIndex]=-1;
    instructionActive[instructionIndex] = false;
    instructionDataArray[instructionIndex].opcode = 0;  
    instructionDataArray[instructionIndex].R1Address = 0;
    instructionDataArray[instructionIndex].R1 = 0;
    instructionDataArray[instructionIndex].R2Address = 0; 
    instructionDataArray[instructionIndex].R2 =
    instructionDataArray[instructionIndex].R3Address = 0;
    instructionDataArray[instructionIndex].R3 = 0;
    instructionDataArray[instructionIndex].shamt = 0;
    instructionDataArray[instructionIndex].imm = 0; 
    instructionDataArray[instructionIndex].address = 0;
    instructionDataArray[instructionIndex].branch = false;
    instructionsStage[instructionIndex]=0;
    return flushed;
}

int main() {
    memoryUnit = malloc(2048 * sizeof(uint32_t));
    
    readInstructions();
    for (int i = 0; i <= sizeof(memoryUnit); i++) {
        printf("instruction%d is %d\n", i,memoryUnit[i]);
    }
    int clockCycle = 1;
    int decodeCount = 0;
    int executeCount = 0;
    int decodeIndex = -1;
    int executeIndex = -1;
    bool flushed = false;
    
    R[1] = 1;
    R[2] = 2;
    R[3] = 3;
    R[4] = 4;
    R[5] = 5;
    R[6] = 6;
    R[7] = 7;
    R[8] = 8;
    R[9] = 9;
    R[10] = 10;
    R[11] = 11;
    R[12] = 12;
    R[13] = 13;
    R[14] = 14;
    R[20] = 20;

    bool runningInstruction = false;
    printf("\n\n\n\n\n");

    while(((memoryUnit[PC]!=0 && PC<1024)) || runningInstruction){
        printf("Clock Cycle: %d \n\n",clockCycle);
        runningInstruction = false;
        
        if(clockCycle%2!=0){
            // every 2 clock cycles we fetch
            fetch(clockCycle);
        }
    
        for(int i=0;i<4;i++){
            if(instructionActive[i]==false){
                char string[50];
                instructionToString(instructions[i], string, sizeof(string));
                // printf("Instruction:  %s index = %d  |      instructionsStage[i]:%d \n\n",string,instructionDataArray[i].instructionAddress+1,instructionsStage[i]); 
                if(instructionsStage[i]==4 ){     //check if we finished the execute stage  --> go to WB
                    flushed = writeback(i);
                }
                if(instructionsStage[i]==3 && clockCycle%2==0){  //check if we finished the execute stage + no fetch operation is being executed
                    printf("Instruction:  %s index = %d   |      Stage: Memory \n\n",string,instructionDataArray[i].instructionAddress+1); //TODO remove address when done testing
                    memory(i);
                    runningInstruction = true;
                }
                if(instructionsStage[i]==2&&(executeIndex==i||executeIndex==-1)){  //check if we finished the decode stage --> go to execute
                    printf("Instruction:  %s index = %d  |      Stage: Execute \n\n",string,instructionDataArray[i].instructionAddress+1); //TODO remove address when done testing
                    executeCount++;
                    executeIndex = i;
                    runningInstruction = true;
                } 
                if(instructionsStage[i]==1&&(decodeIndex==i||decodeIndex==-1)){  //check if we finished the fetch stage
                    printf("Instruction:  %s index = %d  |      Stage: Decode \n\n",string,instructionDataArray[i].instructionAddress+1); //TODO remove address when done testing
                    decodeCount++;  //to check that you completed the 2 clock cycles
                    decodeIndex = i; // to prevent simultaneous decode of instructions
                    runningInstruction = true;
                }
                if(executeCount==2){
                    execute(i);
                    executeCount=0;
                    executeIndex=-1;
                    runningInstruction = true;
                }
                
                if(decodeCount==2){  //to reset the decode count
                    decode(i);
                    decodeCount=0;
                    decodeIndex=-1;
                    runningInstruction = true;
                }
                if(flushed){
                    // When a 
                    decodeCount=0;
                    decodeIndex=-1;
                    executeCount=0;
                    executeIndex=-1;
                    flushed = false;
                }
            }
        }
     
        printf("\n");
        clockCycle++;
        instructionActive[0] = false;
        instructionActive[1] = false;
        instructionActive[2] = false;
        instructionActive[3] = false;

    }
    printf("\n\nRegisters: \n");
    printf("PC = %i \n",PC);
    for(int i=0;i<sizeof(R)/sizeof(uint32_t);i++){
        printf("R%i = %i \n",i ,R[i]);
    }
    printf("\n\nMemory: \n");
    for(int i=0;i<2048;i++){
        if(!(memoryUnit[i]==0)){
            printf("Memory[%d] = %d \n",i,memoryUnit[i]);
        }
    }    

    return 0;
}
