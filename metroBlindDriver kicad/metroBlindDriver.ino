


#include <Arduino.h>


int pinMap[]={3,4,5,6,7,8,9};

bool last=LOW;
void setup() {
  Serial.begin(9600);
  // Set pins 2 to 8 as outputs
  for (int i = 0; i < 7; i++) { 
    pinMode(pinMap[i], OUTPUT); 
  }
  pinMode(0, INPUT);  //ANYRUNNING
  
  pinMode(1, INPUT);  //BUTTON

  pinMode(10, OUTPUT); //DISABLE
  pinMode(14, OUTPUT); //LED
  
}

void setOutput(char* bits)
{

  for (int i = 0; i < 7; i++) { 
    int value=bits[i]=='1'?HIGH:LOW;
    digitalWrite(pinMap[i],value);
  }
}


 char* command="0000000";

  char* commands[]={
  "0000000"//stop roller
  ,"0011010"// Fuori servizio
  ,"1000110"
  ,"0100110"
  ,"0010110"
  ,"0001110"
  ,"1100001"
  ,"1010001"
  ,"0110001"
  ,"1001001"
  ,"0101001"
  ,"0011001"
  ,"1000101"
  ,"0100101"
  ,"0010101"
  ,"0001101"
  ,"1000011"
  ,"0100011"
  ,"0010011"
  ,"0001011"
  ,"0000111"
  ,"1100000"
  ,"1010000"
  ,"0110000"
  ,"1001000"
  ,"0101000"
  ,"1110000"
  ,"1101000" // 27 Anagnina
  ,"1011000"
  ,"0111000" // 29 Cinecittà
  ,"1100100"
  ,"1010100" // 31 Arco di Travertino
  ,"0110100"
  ,"1001100" // 33 San giovanni
  ,"0101100"
  ,"0011100" // 35 Termini
  ,"1100010"
  ,"1010010" // 37 Lepanto
  ,"0110010"
  ,"1001010" // 39 Ottaviano
  ,"0101010"};


  int program[]={01,27,29,31,33,35,37,39};
  int currentProgramPos=0;

void loop() {

 
  delay(200);
  
  if (Serial.available() > 0) {
    char * commandIndex="   ";
    int i = 0;
    while (Serial.available() > 0 && i < 3) {
      char c = Serial.read();
      if (c == '\n') {
        break;  // Break when newline character is received
      }
      commandIndex[i] = c;
      i++;
    }
    Serial.println("New Command: "+String(commandIndex));
    command=commands[String(commandIndex).toInt()];
  } 
  
  digitalWrite(10,HIGH); 

  command=commands[program[currentProgramPos]];

 
  
  for (int i=0;i<7;i++){
    digitalWrite(pinMap[i],command[i]=='1'?HIGH:LOW);  
  }


  
  int running= digitalRead(0);

   if (running==LOW)
   {
    currentProgramPos=currentProgramPos==7?0:currentProgramPos+1;
   
     
    char current[]="0000000";
    digitalWrite(10,LOW);
    
    delay(100);
    for (int i=0;i<7;i++) //FIXME MAKE 7
    {
      for (int j=0;j<7;j++)
      {
        digitalWrite(pinMap[j],LOW);
      }
      delay(100);
      digitalWrite(pinMap[i],HIGH);
      delay(100);
      //Serial.println(String(digitalRead(0)));
      current[i]=digitalRead(0)==HIGH?'0':'1'; 
    }
    for (int i=0;i<7;i++)
    {
        digitalWrite(pinMap[i],LOW);
    }
    Serial.println("stopped at: "+String(current)+" command: "+String(command));
  
   }
   else
   {
    Serial.println(" Running: "+String(running)+ " command: "+String(command));
   }
  

   
}
