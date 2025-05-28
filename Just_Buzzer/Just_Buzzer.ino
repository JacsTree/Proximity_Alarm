const int activeBuzzerPin = 9; 

void setup() {
  Serial.begin(115200);
  Serial.println("Active Buzzer Test");
  pinMode(activeBuzzerPin, OUTPUT);
}

void loop() {
  Serial.println("Active Buzzer ON");
  digitalWrite(activeBuzzerPin, HIGH); 
  delay(500);                         

  Serial.println("Active Buzzer OFF");
  digitalWrite(activeBuzzerPin, LOW);  
  delay(1000);                 
}