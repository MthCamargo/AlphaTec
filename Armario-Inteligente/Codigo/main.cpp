#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

#define SS_PIN    5
#define RST_PIN   22
#define LED       2
#define SDA       21
#define SCL       13

const int i2c_addr = 0x3F;
LiquidCrystal_PCF8574 lcd(i2c_addr); 
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  SPI.begin();           
  rfid.PCD_Init();     

  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();
  lcd.home();
  Serial.begin(115200);

  //DIAGNÓSTICO
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  lcd.setCursor(0, 0);
  lcd.print("Fware MFRC522:0x");
  Serial.println(v, HEX);
  lcd.setCursor(0, 1);
  if (v == 0x00 || v == 0xFF) {
    lcd.print("ERRO");} 
  else {
    lcd.print("Leitor iniciado");}

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(3000);
  digitalWrite(LED, LOW);
}

void loop() {
  delay(300);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aproxime a tag..");
  Serial.println("--------------------------");
  // Verifica se há um novo cartão
  if (!rfid.PICC_IsNewCardPresent()) {return;}

  // Verifica o ID do cartão
  if (!rfid.PICC_ReadCardSerial()) {return;}

  // Quando passar pelos dois ifs
  digitalWrite(LED, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID da Tag: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    lcd.setCursor(0, 1);
    lcd.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    lcd.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println("\n--------------------------");
  delay(3000);
  digitalWrite(LED, LOW);

  // Interrompe a leitura para evitar loop
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}