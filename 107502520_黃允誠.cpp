#include<iostream>
#include<fstream>
#include<vector>
#include<string>
#include<utility>
#include<typeinfo>
#include<bitset>
using namespace std;
// The line for format.
#define line "================================================================="
// Example: bit_int_parse('110001011',3,2) = '10' = 2
template<typename INPUT>
int bit_int_parse(INPUT bit_set,int left,int right){
    return (int)(bitset<32>(bit_set.to_string().substr(bit_set.size()-left-1,left-right+1)).to_ulong());
}
// Clock cycle counter.
static int CC=0;
// Instruction memory.
static vector<bitset<32>> Instruction_memory;
// Fetch the instruction.
bitset<32> Instruction_fetch(int address){
    if(address%4!=0){
        return bitset<32>("00000000000000000000000000000000");
    }
    address/=4;
    if(address>=0&&address<Instruction_memory.size()){
        return Instruction_memory[address];
    }else{
        return bitset<32>("00000000000000000000000000000000");
    }
}
// Get the instruction control signal.
bitset<9> Instruction_control(bitset<32> instruction){
    if(instruction.to_ulong()==0){
        return bitset<9>("000000000");
    }else{
        switch (bit_int_parse(instruction,31,26)){
            case 0:
                return bitset<9>("110000010");
            case 4:
                return bitset<9>("001010000");
            case 8:
                return bitset<9>("000100010");
            case 12:
                return bitset<9>("011100010");
            case 35:
                return bitset<9>("000101011");
            case 43:
                return bitset<9>("000100100");
        }
    }
}
// Register
class Register_def{
public:
    int Value[10]={0,9,5,7,1,2,3,4,5,6};
    void Initialize(){
        Value[0]=0;
        Value[1]=9;
        Value[2]=5;
        Value[3]=7;
        Value[4]=1;
        Value[5]=2;
        Value[6]=3;
        Value[7]=4;
        Value[8]=5;
        Value[9]=6;
    }
    void Write(char register_write_signal,int write_register,int write_data){
        if(register_write_signal){
            Value[write_register]=write_data;
        }
    }
    void Read(int read_register_1,int read_register_2,int&read_flow_1,int&read_flow_2){
        read_flow_1=Value[read_register_1];
        read_flow_2=Value[read_register_2];
    }
};
static Register_def Register;
// Data memory
class Data_memory_def{
public:
    int Value[5]={5,9,4,8,7};
    void Initialize(){
        Value[0]=5;
        Value[1]=9;
        Value[2]=4;
        Value[3]=8;
        Value[4]=7;
    }
    void Reference(char memory_write_signal,char memory_read_signal,int address,int write_data,int&read_flow){
        if(address%4!=0||!(address/4>=0&&address/4<=4)){
            read_flow=0;
            return;
        }
        if(memory_write_signal){
            Value[address/4]=write_data;
        }
        if(memory_read_signal){
            read_flow=Value[address/4];
        }else{
            read_flow=0;
        }
    }
};
static Data_memory_def Data_memory;
// IF/ID register
struct IFIDdef{
    int PC=0;
    bitset<32> Instruction=bitset<32>("00000000000000000000000000000000");
};
static IFIDdef IFID_cur;
static IFIDdef IFID_next;
// ID/EX register
struct IDEXdef{
    int ReadData1=0;
    int ReadData2=0;
    int sign_ext=0;
    int Rs=0;
    int Rt=0;
    int Rd=0;
    bitset<9> Control_signals=bitset<9>("000000000");
    bitset<9>::reference RegDst=Control_signals[8];
    bitset<9>::reference ALUOp1=Control_signals[7];
    bitset<9>::reference ALUOp0=Control_signals[6];
    bitset<9>::reference ALUSrc=Control_signals[5];
    bitset<9>::reference Branch=Control_signals[4];
    bitset<9>::reference MemRead=Control_signals[3];
    bitset<9>::reference MemWrite=Control_signals[2];
    bitset<9>::reference RegWrite=Control_signals[1];
    bitset<9>::reference MemtoReg=Control_signals[0];
};
static IDEXdef IDEX_cur;
static IDEXdef IDEX_next;
// EX/MEM register
struct EXMEMdef{
    int ALUout=0;
    int WriteData=0;
    int RtRd=0;
    bitset<5> Control_signals=bitset<5>("00000");
    bitset<5>::reference Branch=Control_signals[4];
    bitset<5>::reference MemRead=Control_signals[3];
    bitset<5>::reference MemWrite=Control_signals[2];
    bitset<5>::reference RegWrite=Control_signals[1];
    bitset<5>::reference MemtoReg=Control_signals[0];
};
static EXMEMdef EXMEM_cur;
static EXMEMdef EXMEM_next;
// MEM/WB register
struct MEMWBdef{
    int ReadData=0;
    int ALUout=0;
    int RtRd=0;
    bitset<2> Control_signals=bitset<2>("00");
    bitset<2>::reference RegWrite=Control_signals[1];
    bitset<2>::reference MemtoReg=Control_signals[0];
};
static MEMWBdef MEMWB_cur;
static MEMWBdef MEMWB_next;
// Forwarding control unit
class Forwarding_unit_def{
public:
    bitset<2> Forwarding_A=bitset<2>("00");
    bitset<2> Forwarding_B=bitset<2>("00");
    int Actual_ReadData1=0;
    int Actual_ReadData2=0;
    void Initialize(){
        Forwarding_A=Forwarding_B=bitset<2>("00");
        Actual_ReadData1=Actual_ReadData2=0;
    }
    void Detect(){
        Forwarding_A[1]=(EXMEM_cur.RegWrite&&EXMEM_cur.RtRd==IDEX_cur.Rs);
        Forwarding_A[0]=((!Forwarding_A[1])&&MEMWB_cur.RegWrite&&MEMWB_cur.RtRd==IDEX_cur.Rs);
        Forwarding_B[1]=(EXMEM_cur.RegWrite&&EXMEM_cur.RtRd==IDEX_cur.Rt);
        Forwarding_B[0]=((!Forwarding_B[1])&&MEMWB_cur.RegWrite&&MEMWB_cur.RtRd==IDEX_cur.Rt);
    }
    void Decide(){
        switch (Forwarding_A.to_ulong()){
            case 0:
                Actual_ReadData1=IDEX_cur.ReadData1;
                break;
            case 1:
                Actual_ReadData1=(MEMWB_cur.MemtoReg?MEMWB_cur.ReadData:MEMWB_cur.ALUout);
                break;
            case 2:
                Actual_ReadData1=EXMEM_cur.ALUout;
                break;
        }
        switch (Forwarding_B.to_ulong()){
            case 0:
                Actual_ReadData2=IDEX_cur.ReadData2;
                break;
            case 1:
                Actual_ReadData2=(MEMWB_cur.MemtoReg?MEMWB_cur.ReadData:MEMWB_cur.ALUout);
                break;
            case 2:
                Actual_ReadData2=EXMEM_cur.ALUout;
                break;
        }
    }
};
static Forwarding_unit_def Forwarding_unit;
// Load word hazard detection unit
class Hazard_detection_unit_def{
public:
    bool Stall=false;
    void Initialize(){
        Stall=false;
    }
    void Detect(){
        Stall=(IDEX_cur.MemRead&&(IDEX_cur.Rt==bit_int_parse(IFID_cur.Instruction,25,21)||IDEX_cur.Rt==bit_int_parse(IFID_cur.Instruction,20,16)));
    }
    void Decide(){
        if(Stall){
            IDEX_next.Control_signals=bitset<9>("000000000");
            IFID_next.PC=IFID_cur.PC;
            IFID_next.Instruction=IFID_cur.Instruction;
        }
    }
};
static Hazard_detection_unit_def Hazard_detection_unit;
// Branch control unit
class Branch_unit_def{
public:
    bool Branch=false;
    int Target_PC=0;
    void Initialize(){
        Branch=false;
        Target_PC=0;
    }
    void Calculate(){
        Branch=(IDEX_next.Branch&&IDEX_next.ReadData1==IDEX_next.ReadData2);
        Target_PC=IFID_cur.PC+IDEX_next.sign_ext*4;
    }
    void Decide(){
        if(Branch){
            IFID_cur.PC=Target_PC;
            IFID_cur.Instruction=bitset<32>("00000000000000000000000000000000");
        }
    }
};
static Branch_unit_def Branch_unit;
// ALU
class ALU_def{
public:
    bitset<3> Calculate_ALUctr(){
        if(IDEX_cur.ALUOp1&&IDEX_cur.ALUOp0){
            return bitset<3>("000");
        }else{
            bitset<3> ALUctr;
            bitset<6> Function(IDEX_cur.sign_ext%64);
            ALUctr[0]=((Function[0]||Function[3])&&IDEX_cur.ALUOp1);
            ALUctr[1]=(!(Function[2]&&IDEX_cur.ALUOp1));
            ALUctr[2]=((Function[1]&&IDEX_cur.ALUOp1)||IDEX_cur.ALUOp0);
            return ALUctr;
        }
    }
    int Operate(int ALUin_a,int ALUin_b){
        switch (Calculate_ALUctr().to_ulong()){
            case 0:
                return (int)((bitset<32>(ALUin_a)&bitset<32>(ALUin_b)).to_ulong());
            case 1:
                return (int)((bitset<32>(ALUin_a)|bitset<32>(ALUin_b)).to_ulong());
            case 2:
                return ALUin_a+ALUin_b;
            case 6:
                return ALUin_a-ALUin_b;
            case 7:
                return (ALUin_a<ALUin_b?1:0);
        }
    }
};
static ALU_def ALU;
// Initialize all simulation data.
void Initialize(){
    Instruction_memory.clear();
    CC=0;
    Register.Initialize();
    Data_memory.Initialize();
    Forwarding_unit.Initialize();
    Branch_unit.Initialize();
    Hazard_detection_unit.Initialize();
    IFID_cur.PC=0;
    IFID_cur.Instruction=bitset<32>("00000000000000000000000000000000");
    IDEX_cur.ReadData1=0;
    IDEX_cur.ReadData2=0;
    IDEX_cur.sign_ext=IDEX_cur.Rs=IDEX_cur.Rt=IDEX_cur.Rd=0;
    IDEX_cur.Control_signals=bitset<9>("000000000");
    EXMEM_cur.ALUout=EXMEM_cur.WriteData=EXMEM_cur.RtRd=0;
    EXMEM_cur.Control_signals=bitset<5>("00000");
    MEMWB_cur.ReadData=MEMWB_cur.ALUout=MEMWB_cur.RtRd=0;
    MEMWB_cur.Control_signals=bitset<2>("00");
    IFID_next=IFID_cur;
    IDEX_next=IDEX_cur;
    EXMEM_next=EXMEM_cur;
    MEMWB_next=MEMWB_cur;
}
// Simulation with given input and output file names.
void Simulate(string input_file_name,string output_file_name){
    Initialize();
    // Construct the Instruction memory from input file.
    fstream input(input_file_name,fstream::in);
    bitset<32> next;
    while(input>>next){
        Instruction_memory.push_back(next);
    }
    input.close();
    fstream output(output_file_name,fstream::out);
    // Each clock cycle...
    do{
        // Forward and Load word hazard detection for this clock cycle.
        Forwarding_unit.Detect();
        Hazard_detection_unit.Detect();
        // Branch operation based on Branch calculations made in previous clock cycle.
        Branch_unit.Decide();
        // Most of data path flow.
        Register.Write(MEMWB_cur.RegWrite,MEMWB_cur.RtRd,(MEMWB_cur.MemtoReg?MEMWB_cur.ReadData:MEMWB_cur.ALUout));
        Data_memory.Reference(EXMEM_cur.MemWrite,EXMEM_cur.MemRead,EXMEM_cur.ALUout,EXMEM_cur.WriteData,MEMWB_next.ReadData);
        MEMWB_next.ALUout=EXMEM_cur.ALUout;
        MEMWB_next.RtRd=EXMEM_cur.RtRd;
        MEMWB_next.Control_signals=bitset<2>(bit_int_parse(EXMEM_cur.Control_signals,1,0));
        // ALU inputs are selected by Forwarding control unit.
        Forwarding_unit.Decide();
        // Most of data path flow (continued).
        EXMEM_next.ALUout=ALU.Operate(Forwarding_unit.Actual_ReadData1,(IDEX_cur.ALUSrc?IDEX_cur.sign_ext:Forwarding_unit.Actual_ReadData2));
        EXMEM_next.WriteData=Forwarding_unit.Actual_ReadData2;
        EXMEM_next.RtRd=(IDEX_cur.RegDst?IDEX_cur.Rd:IDEX_cur.Rt);
        EXMEM_next.Control_signals=bitset<5>(bit_int_parse(IDEX_cur.Control_signals,4,0));
        IDEX_next.Rs=bit_int_parse(IFID_cur.Instruction,25,21);;
        IDEX_next.Rt=bit_int_parse(IFID_cur.Instruction,20,16);
        IDEX_next.Rd=bit_int_parse(IFID_cur.Instruction,15,11);
        Register.Read(IDEX_next.Rs,IDEX_next.Rt,IDEX_next.ReadData1,IDEX_next.ReadData2);
        IDEX_next.sign_ext=bit_int_parse(IFID_cur.Instruction,15,0);
        IDEX_next.Control_signals=Instruction_control(IFID_cur.Instruction);
        IFID_next.PC=IFID_cur.PC+4;
        IFID_next.Instruction=Instruction_fetch(IFID_cur.PC);
        // Stall for Load word hazard?
        Hazard_detection_unit.Decide();
        // Branch calculations (If Branch taken, operations will be simulated in the beginning of next clock cycle.)
        Branch_unit.Calculate();
        // There will be candidates expected when we try to simulate electronic physical data flow with sequential instruction.
        // This is my solution.
        IFID_cur=IFID_next;
        IDEX_cur=IDEX_next;
        EXMEM_cur=EXMEM_next;
        MEMWB_cur=MEMWB_next;
        // output simulation result for this clock cycle
        output<<"CC "<<++CC<<":\n\n";
        output<<"Registers:\n";
        for(int i=0;i<10;i++){
            output<<"$"<<i<<": "<<Register.Value[i]<<"\n";
        }
        output<<"\n";
        output<<"Data memory:\n0x00: "<<Data_memory.Value[0]<<"\n0x04: "<<Data_memory.Value[1]<<"\n0x08: "<<Data_memory.Value[2]<<"\n0x0C: "<<Data_memory.Value[3]<<"\n0x10: "<<Data_memory.Value[4]<<"\n\n";
        output<<"IF/ID :\nPC\t\t"<<IFID_cur.PC<<"\nInstruction\t"<<IFID_cur.Instruction<<"\n\n";
        output<<"ID/EX :\nReadData1\t"<<IDEX_cur.ReadData1<<"\nReadData2\t"<<IDEX_cur.ReadData2<<"\nsign_ext\t"<<IDEX_cur.sign_ext<<"\nRs\t\t"<<IDEX_cur.Rs<<"\nRt\t\t"<<IDEX_cur.Rt<<"\nRd\t\t"<<IDEX_cur.Rd<<"\nControl signals\t"<<IDEX_cur.Control_signals<<"\n\n";
        output<<"EX/MEM :\nALUout\t\t"<<EXMEM_cur.ALUout<<"\nWriteData\t"<<EXMEM_cur.WriteData<<"\nRt/Rd\t\t"<<EXMEM_cur.RtRd<<"\nControl signals\t"<<EXMEM_cur.Control_signals<<"\n\n";
        output<<"MEM/WB :\nReadData\t"<<MEMWB_cur.ReadData<<"\nALUout\t\t"<<MEMWB_cur.ALUout<<"\nRt/Rd\t\t"<<MEMWB_cur.RtRd<<"\nControl signals\t"<<MEMWB_cur.Control_signals<<"\n";
        output<<line<<"\n";
    }while(!(IFID_cur.Instruction.none()&&IDEX_cur.ReadData1==0&&IDEX_cur.ReadData2==0&&IDEX_cur.sign_ext==0&&IDEX_cur.Rs==0&&IDEX_cur.Rt==0&&IDEX_cur.Rd==0&&IDEX_cur.Control_signals.none()&&EXMEM_cur.ALUout==0&&EXMEM_cur.WriteData==0&&EXMEM_cur.RtRd==0&&EXMEM_cur.Control_signals.none()&&MEMWB_cur.ReadData==0&&MEMWB_cur.ALUout==0&&MEMWB_cur.RtRd==0&&MEMWB_cur.Control_signals.none()));
    output.close();
}
// Simulation for problems 1~4.
int main(){
//  Simulate("SampleInput.txt","SampleOutput.txt");
    Simulate("General.txt","genResult.txt");
    Simulate("Datahazard.txt","dataResult.txt");
    Simulate("Lwhazard.txt","loadResult.txt");
    Simulate("Branchhazard.txt","branchResult.txt");
}
