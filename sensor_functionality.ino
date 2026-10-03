//turbidity
int turbidityPin = 4;

//ph 
#include "DFRobot_PH.h"
#include <EEPROM.h>
int phPin = 5;
float voltage,phValue;
DFRobot_PH ph;

//temperature
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 10
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temperature = 0;

//tds
#include "GravityTDS.h"
GravityTDS gravityTds;
int tdsPin = 8;
int tdsValue = 0;

//distance
int trigPin = 12;
int echoPin = 13;

void setup() {
Serial.begin(115200);
//ph
ph.begin();

//tds
gravityTds.setPin(tdsPin);
gravityTds.setAref(3.3);  
gravityTds.setAdcRange(4096); 
gravityTds.begin();

//distance
pinMode(trigPin, OUTPUT);
pinMode(echoPin, INPUT_PULLUP);
digitalWrite(echoPin, HIGH);
}

void loop() {
//turbidity
int turbidityValue = analogRead(turbidityPin);
int turbidity = map(turbidityValue,0,700,100,0);
Serial.print("turbidity = ");
Serial.println(turbidity);

//ph
  static unsigned long timepoint = millis();
  if(millis()-timepoint>1000U){                 
      timepoint = millis();
      temperature = readTemperature();         
      voltage = analogRead(phPin)/1024.0*5000; 
      phValue = ph.readPH(voltage,temperature); 
      Serial.print("temperature:");
      Serial.println(temperature,1);
      Serial.print("^C  pH:");
      Serial.println(phValue,2);
    }
    ph.calibration(voltage,temperature); 

//tds
    gravityTds.setTemperature(temperature);  
    gravityTds.update();  
    tdsValue = gravityTds.getTdsValue(); 
    Serial.print("tds value = ");
    Serial.println(tdsValue,0);
    Serial.print("ppm");
    delay(1000);

//distance
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(15);
  digitalWrite(trigPin, LOW);
  int distance = pulseIn(echoPin, HIGH, 26000);
  distance=distance/58;

  Serial.print("distance = ");
  Serial.println(distance);
  Serial.println(" cm");
  delay(50);

delay(1500);

}

//temperature
float readTemperature(){
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}