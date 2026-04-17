#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Pinos RFID
#define SS_PIN    5
#define RST_PIN   4
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23

#define LED       2
#define RX2       16
#define SDA_PIN   21
#define SCL_PIN   22

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);
  pinMode(RX2, OUTPUT);
  digitalWrite(RX2, HIGH);
  Serial.println("PORTA FECHADA");
  
  // Inicialização
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  SPI.begin();           
  rfid.PCD_Init();     

  // Diagnósticos
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  lcd.setCursor(0, 0);
  lcd.print("Firmware: 0x");
  lcd.print(v, HEX);
  Serial.println("Firmware: 0x");
  Serial.println(v, HEX);
  
  if (v == 0x00 || v == 0xFF) {
    lcd.setCursor(0, 1);
    lcd.print("Erro no RFID");
    Serial.println("Erro no RFID");
    while(true); // Trava se RFID não conectado
  } else {
    lcd.setCursor(0, 1);
    lcd.print("RFID OK");
    Serial.println("RFID OK");
    delay(3000);
  }

  pinMode(LED, OUTPUT);
  lcd.clear();
  lcd.print("Aproxime a tag..");
}

void loop() {
  // Verifica se há novo cartão
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
    //delay(100);-------------------------------------------------------------
  }

  // Se tag foi lida
  digitalWrite(RX2, LOW);
  Serial.println("PORTA ABERTA");
  digitalWrite(LED, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID detectado:");
  
  lcd.setCursor(0, 1);
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    // ID em Hexadecimal
    if(rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
    uidStr += " ";
  }
  uidStr.toUpperCase();
  lcd.print(uidStr);
  
  Serial.print("Tag ID: ");
  Serial.println(uidStr);

  delay(3000);
  
  // Reseta para o estado inicial
  digitalWrite(RX2, HIGH);
  Serial.println("PORTA FECHADA");
  digitalWrite(LED, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aproxime a tag..");

  // Interrompe a leitura para evitar loop na mesma tag
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}