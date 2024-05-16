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
};
struct instructionData instructionDataArray[4]; 

void flushInstructions(int branchInstructionIndex){
    int branchInstructionAddress = instructionDataArray[branchInstructionIndex].instructionAddress;
    for(int i=0;i<4;i++){
        int instructionAddress =  instructionDataArray[i].instructionAddress;
        if(i!=branchInstructionIndex && instructionAddress>branchInstructionAddress){
            // 1st condition: Flush the instructions that is not the branch one
            // &&
            // 2nd condition: Flush the instructions that came after the branch not before
            instructions[i] = -1;
            instructionsStage[i] = 0; 
            instructionActive[i] = false;
        }  
    }
    
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

                token = strtok(NULL, " - ");
                reg_num = strtol(token, &ptr, 10);
                printf("value of imm is %d: %d\n", reg_num, reg_num);
                instruction += reg_num;
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

uint32_t concatenate_bits(uint32_t PC, uint32_t imm) {
    // TODO TEST
    // Extract the desired bits from PC and imm
    uint32_t PC_bits = PC & 0xF0000000;  // PC[31:28]
    uint32_t imm_bits = imm & 0x0FFFFFFF;   // imm[27:0]

    // Concatenate the extracted bits
    uint32_t concatenated_value = PC_bits | imm_bits;

    return concatenated_value;
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

int counter = 1;
void fetch() {
    // In description says that we fetch from memory
    int i=0;
    while(instructionsStage[i]!=0 && i<4){
        i++;
    }
    if(i>=4) return;
    instructions[i] = memoryUnit[PC]; // TODO UNCOMMENT
    // instructions[i] = counter;
    instructionDataArray[i].instructionAddress = PC;
    counter++;
    PC = PC + 1;
    instructionsStage[i] = 1;
    instructionActive[i] = true;
    char string[50];
    instructionToString(instructions[i], string, sizeof(string));
    
    printf("Instruction:  %s     |      Stage: Fetch \n",string);
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
    int R1;
    int R1Address;
    int R2;
    int R3;
    int shamt;
    int imm;
    int res;
    int address;
    switch (opcode)
    {
        case 0:
            // ADD
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            R3 = instructionDataArray[instructionIndex].R3;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("ADD instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            // ASK IF NEED TO DO ERRORS OR IF SEGMENTATION FAULTS ARE ACCEPTED
            if(R1Address!=0){
                R[R1Address] = R2 + R3;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            break;
        case 1:
            // SUB
            
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            R3 = instructionDataArray[instructionIndex].R3;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("SUB instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            // ASK IF NEED TO DO ERRORS OR IF SEGMENTATION FAULTS ARE ACCEPTED
            if(R1Address!=0){
                R[R1Address] = R2 - R3;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            
            break;
        case 2:
            // MULI
            
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("MULI instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            if(R1Address!=0){
                R[R1Address] = R2 * imm;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            
            break;

        case 3:
            // ADDI
            
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("ADDI instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            if(R1Address!=0){
                R[R1Address] = R2 + imm;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            
            break;

        case 4:
            // BNE
            
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            printf("BNE instruction \n");
            printf("PC was %i in execute stage \n",PC);
            res = R1-R2;
            if(res!=0){
                PC = PC + imm;  //pc + 1 is already done so we'll add the imm directly --> check the remarks document
                flushInstructions(instructionIndex);
                printf("PC has changed to %i in execute stage \n",PC);
            }else{
                printf("PC has not been changed \n");
            }     
            
            break;

        case 5:
            // ANDI
            
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("ANDI instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            if(R1Address!=0){
                R[R1Address] = R2 & imm;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            
            break;

        case 6:
            // ORI
            
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("ORI instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            if(R1Address!=0){
                R[R1Address] = R2 | imm;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            
            break;

        case 7:
            // J
            
            address = instructionDataArray[instructionIndex].address;
            printf("J instruction \n");
            printf("PC was %i in execute stage \n",PC);
            int temp = concatenate_bits(PC,address);
            PC = temp;
            flushInstructions(instructionIndex);
            printf("PC has changed to %i in execute stage \n",temp);
            break;

        case 8:
            // SLL
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            shamt = instructionDataArray[instructionIndex].shamt;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("SLL instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            if(R1Address!=0){
                R[R1Address] = R2 << shamt;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            break;

        case 9:
            // SRL
            R1 = instructionDataArray[instructionIndex].R1;
            R2 = instructionDataArray[instructionIndex].R2;
            shamt = instructionDataArray[instructionIndex].shamt;
            R1Address = instructionDataArray[instructionIndex].R1Address;
            printf("SRL instruction \n");
            printf("R%i was %i in execute stage \n",R1Address,R1);
            if(R1Address!=0){
                R[R1Address] = R2 >> shamt;
                printf("R%i has changed to %i in execute stage \n",R1Address,R[R1Address]);
            }else{
                printf("R0 has not been changed \n");
            }
            
            break;

        case 10:
            // LW            
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            // R1 = memoryUnit[R2+imm];     --> this accesses the memory which is incorrect in execute stage
            //loadStoreAddress holds R2+imm (ALU operation)
            instructionDataArray[instructionIndex].loadStoreAddress = R2 + imm; 
            printf("LW instruction \n");
            //previous and current values won't be printed here --> will be printed in memory stage
            break;

        case 11:
            // SW
            R2 = instructionDataArray[instructionIndex].R2;
            imm = instructionDataArray[instructionIndex].imm;
            instructionDataArray[instructionIndex].loadStoreAddress = R2 + imm;
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
        printf("R%i was %i in Memory stage \n",R1Address,R1);
        R[R1Address] = memoryUnit[memoryLocation];
        printf("R%i has changed to %i in Memory stage \n",R1Address,R[R1Address]);
    }else if(opcode==11){
        // SW
        // since only memory is LW and SW
        printf("Memory address location %i was %i in Memory stage \n",memoryLocation,memoryUnit[memoryLocation]);
        memoryUnit[memoryLocation] = R1;
        printf("Memory address location %i has changed to %i in Memory stage \n",memoryLocation,memoryUnit[memoryLocation]);
    }
    instructionsStage[instructionIndex] += 1;
}

// TA Ahmed Essam said that we dont implement it for package 1
void writeback(int32_t instructionIndex){ 
    //TODO R1 value is not correct
    char *opcodes[] = {"ADD ", "SUB ", "MULI", "ADDI", "ANDI", "ORI ", "SLL ","SRL "};
    char string[50];
    instructionToString(instructions[instructionIndex], string, sizeof(string));
    printf("Instruction:  %s     |      Stage: Writeback",string);
    if(isFirst4CharactersInArray(string, opcodes, sizeof(opcodes) / sizeof(opcodes[0]))) {     
        int R1Address = instructionDataArray[instructionIndex].R1Address;     
        memoryUnit[R1Address + 1024] = R[R1Address];
        printf(": %d was stored in memory address %d\n",memoryUnit[R1Address + 1024],1024+R1Address);
    } else {
        printf(" (not needed)\n");
    }
    instructionsStage[instructionIndex]=0;
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
    int decodeFlag = -1;
    int executeFlag = -1;
    
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

    printf("\n\n\n\n\n");

    while(clockCycle<=19){
        printf("Clock Cycle: %d \n\n",clockCycle);
    
        // FETCH
        if(clockCycle%2!=0){
            // every 2 clock cycles we fetch
            fetch();
        }
        for(int i=0;i<4;i++){
            if(instructionActive[i]==false){
                char string[50];
                instructionToString(instructions[i], string, sizeof(string));
                
                
                if(instructionsStage[i]==4){     //check if we finished the execute stage  --> go to WB
                    writeback(i);
                }
                if(instructionsStage[i]==3 && clockCycle%2==0){  //check if we finished the execute stage + no fetch operation is being executed
                    printf("Instruction:  %s   |      Stage: Memory \n\n",string);
                    memory(i);
                }
                if(instructionsStage[i]==2&&(executeFlag==i||executeFlag==-1)){  //check if we finished the decode stage --> go to execute
                    printf("Instruction:  %s   |      Stage: Execute \n\n",string);
                    executeCount++;
                    executeFlag = i;
                }
                if(instructionsStage[i]==1&&(decodeFlag==i||decodeFlag==-1)){  //check if we finished the fetch stage
                    printf("Instruction:  %s   |      Stage: Decode \n\n",string);
                    decodeCount++;  //to check that you completed the 2 clock cycles
                    decodeFlag = i; // to prevent simultaneous decode of instructions
                }
                if(executeCount==2){
                    execute(i);
                    executeCount=0;
                    executeFlag=-1;
                }
                
                if(decodeCount==2){  //to reset the decode count
                    decode(i);
                    decodeCount=0;
                    decodeFlag=-1;
                }
            }
        }
        instructionActive[0] = false;
        instructionActive[1] = false;
        instructionActive[2] = false;
        instructionActive[3] = false;
        
        // if(clockCycle%2==0){
        //     // every 2 clock cycles we fetch
        //     for(int i=0;i<4;i++){
        //         if(instructionsStage[i]==3){
        //             mem(i);
        //         }
        //     }
        // }


        // decode takes 2 clocks
        // execute takes 2 clocks
        // write back cant be done with fetch
        printf("\n");
        clockCycle++;
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
