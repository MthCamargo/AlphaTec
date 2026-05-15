#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>

// CONFIGURAÇÕES GERAIS E CREDENCIAIS

const char* URL_SCRIPT = "https://script.google.com/macros/s/AKfycbx9lcV2db8lJ3g8lSgPuuou4h2SoBKJvsoII6p10UAbn-eGmM3Rr0FSHTGZygqSb2_K/exec"; 
const String TAG_MESTRA = "91D9FBA9"; 

const char* WIFI_SSID = "Dexinho";
const char* WIFI_PASS = "MatheusC";

// MAPEAMENTO DE PINOS (ESP32)

#define SS_PIN    5
#define RST_PIN   4     
#define SCK_PIN   18
#define MISO_PIN  19
#define MOSI_PIN  23

#define LED_PIN   2
#define SOLENOIDE 16    
#define SDA_PIN   21
#define SCL_PIN   22

// OBJETOS E VARIÁVEIS GLOBAIS

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

int modoSistema = 0;          // 0: Normal, 1: Admin Operador, 2: Admin Ferramenta
String ultimoOperadorNome = ""; 
bool trancaAberta = false;
unsigned long tempoAbertura = 0;
const long TIMEOUT_TRANCA = 15000; // tempo para fechar automaticamente

// FUNÇÕES DE HARDWARE E INTERFACE

void displayMsg(String linha1, String linha2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(linha1);
  lcd.setCursor(0, 1);
  lcd.print(linha2);
}

void abrirTranca() {
  digitalWrite(SOLENOIDE, LOW); 
  digitalWrite(LED_PIN, HIGH);
  trancaAberta = true;
  tempoAbertura = millis(); // Inicia o cronômetro
  Serial.println("[HARDWARE] Solenoide ATIVADA -> PORTA ABERTA");
}

void fecharTranca() {
  digitalWrite(SOLENOIDE, HIGH); 
  digitalWrite(LED_PIN, LOW);
  trancaAberta = false;
  ultimoOperadorNome = ""; // Limpa a memória do operador ao trancar
  Serial.println("[HARDWARE] Solenoide DESATIVADA -> PORTA FECHADA");
}

// FUNÇÕES DE COMUNICAÇÃO (WIFI / PLANILHA)

String realizarRequisicao(String parametros) {
  WiFiClientSecure client;
  client.setInsecure(); // Ignora verificação de certificado SSL
  HTTPClient http;
  
  // Anexa o nome do operador atual à requisição (se houver)
  String link = String(URL_SCRIPT) + "?" + parametros + "&OP_NOME=" + ultimoOperadorNome;
  link.replace(" ", "%20");

  if (http.begin(client, link)) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); 
    int code = http.GET();
    if (code > 0) {
      String payload = http.getString();
      http.end();
      return payload;
    }
    http.end();
  }
  return "ERRO";
}

// LÓGICA: MODO NORMAL

void fluxoNormal(String id) {
  Serial.println("[SISTEMA] Consultando servidor para a Tag: " + id);
  displayMsg("Consultando...", "Aguarde");
  
  String res = realizarRequisicao("ID=" + id);
  Serial.println("[SISTEMA] Resposta do Servidor: " + res);

  // Lógica se a tag for de um OPERADOR
  if (res.startsWith("OP:")) {
    ultimoOperadorNome = res.substring(3);
    Serial.println("[ACESSO] Operador reconhecido: " + ultimoOperadorNome);
    abrirTranca();
    displayMsg("Ola, " + ultimoOperadorNome, "Acesso Liberado");
    delay(3000);
  } 
  // Lógica se a tag for de uma FERRAMENTA
  else if (res.startsWith("FE:")) {
    if (!trancaAberta) {
      // Tentar ler ferramenta com a porta fechada
      Serial.println("[ERRO] Tentativa de registrar ferramenta sem operador logado.");
      displayMsg("Identifique-se", "Primeiro!");
      delay(3000);
    } else {
      // Porta está aberta e operador está logado
      int p1 = res.indexOf(':'); int p2 = res.lastIndexOf(':');
      String nomeF = res.substring(p1 + 1, p2);
      String acao = res.substring(p2 + 1);
      
      Serial.println("[ACESSO] Ferramenta [" + nomeF + "] " + acao + " por " + ultimoOperadorNome);
      fecharTranca();
      displayMsg(nomeF, acao + "!");
      delay(3000);
    }
  } 
  // Respostas de Erro do Servidor
  else if (res == "SEM_OPERADOR") {
    Serial.println("[ACESSO NEGADO] Planilha recusou (sem operador).");
    displayMsg("Acesso Negado", "Passe seu Cracha");
    delay(3000);
  }
  else {
    Serial.println("[ACESSO NEGADO] Tag desconhecida ou não cadastrada.");
    displayMsg("Tag Invalida", "Nao Cadastrada");
    delay(3000);
  }

  // Atualiza o LCD para o estado atual
  if (trancaAberta) {
    displayMsg("Passe Ferramenta", "ou aguarde...");
  } else {
    displayMsg("Aproxime a tag..");
  }
}

// LÓGICA: MODO ADMIN

void fluxoAdmin(String id, String tipo) {
  Serial.println("[ADMIN] Verificando base de " + tipo + " para a Tag: " + id);
  displayMsg("Lendo Base...", tipo);
  
  String status = realizarRequisicao("ID=" + id + "&TIPO=" + tipo);

  if (status == "PENDENTE") {
    Serial.println("[ADMIN] Tag livre. Pronta para registro.");
    displayMsg("Nova Tag", "Digite no Serial");
    
    Serial.print(">> DIGITE O NOME DO(A) " + tipo);
    while(Serial.available()) Serial.read(); // Limpa buffer
    while(Serial.available() == 0) { delay(100); } // Aguarda digitação
    String nome = Serial.readStringUntil('\n'); 
    nome.trim();
    
    if (nome.length() > 0) {
      Serial.println("\n[ADMIN] Enviando cadastro de: " + nome);
      displayMsg("Cadastrando...", nome);
      String resposta = realizarRequisicao("ID=" + id + "&NOME=" + nome + "&TIPO=" + tipo);
      Serial.println("[ADMIN] Resposta do cadastro: " + resposta);
      displayMsg("Sucesso!", "Cadastrado");
    }
  } else {
    Serial.println("[ADMIN] Tag já registrada como: " + status);
    displayMsg(status, "S:Apagar N:Sair");
    
    Serial.print(">> REMOVER REGISTRO? (S) SIM / (N) CANCELAR: ");
    while(Serial.available()) Serial.read();
    while(Serial.available() == 0) { delay(100); }
    String opt = Serial.readStringUntil('\n'); 
    opt.toUpperCase();
    
    if (opt.indexOf("R") >= 0) {
      Serial.println("\n[ADMIN] Solicitando remocao do banco...");
      displayMsg("Removendo...");
      String resposta = realizarRequisicao("ID=" + id + "&ACAO=DELETE&TIPO=" + tipo);
      Serial.println("[ADMIN] Resposta da remocao: " + resposta);
      displayMsg("Removido!");
    } else {
      Serial.println("\n[ADMIN] Operacao cancelada pelo usuario.");
      displayMsg("Cancelado", "");
    }
  }
  
  delay(2500);
  displayMsg("MODO ADMIN", tipo);
}

//============================== SETUP ===============================

void setup() {
  Serial.begin(115200);
  
  pinMode(SOLENOIDE, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  fecharTranca();
  
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  displayMsg("Iniciando...", "Aguarde Wi-Fi");

  SPI.begin();           
  rfid.PCD_Init();     
  
  // Diagnóstico RFID
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print("[DIAGNOSTICO] Firmware RFID: 0x");
  Serial.println(v, HEX);
  if (v == 0x00 || v == 0xFF) {
    Serial.println("[ERRO CRITICO] Modulo RFID nao detectado.");
    displayMsg("Erro de Hardware", "Verifique RFID");
    while(true); // Trava o sistema se erro
  }

  // Conexão Wi-Fi
  Serial.print("[WIFI] Conectando a ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\n[WIFI] Conectado! IP: " + WiFi.localIP().toString());
  
  Serial.println("========================================");
  Serial.println("   SISTEMA ONLINE - MODO NORMAL         ");
  Serial.println("========================================");
  displayMsg("SISTEMA PRONTO", "Aproxime a tag..");
}

//============================== LOOP ===============================

void loop() {
  
  // VERIFICAÇÃO DE TIMEOUT DA TRANCA
  if (trancaAberta && (millis() - tempoAbertura >= TIMEOUT_TRANCA)) {
    Serial.println("[TIMEOUT] tempo esgotado, sem leitura de ferramenta. Fechando tranca...");
    fecharTranca();
    displayMsg("Tempo Esgotado!", "Trancado.");
    delay(3000);
    displayMsg("Aproxime a tag..");
  }

  // LEITURA DE CARTÃO RFID
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  // Converte a UID paraHexadecimal
  String idLida = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    idLida += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    idLida += String(rfid.uid.uidByte[i], HEX);
  }
  idLida.toUpperCase();

  Serial.println("\n---------------------------------------");
  Serial.println("[RFID] Tag detectada: " + idLida);

  // CONTROLE DE MODOS VIA TAG MESTRA
  if (idLida == TAG_MESTRA) {
    modoSistema = (modoSistema + 1) % 3;
    
    // Fecha a tranca ao mudar de modo
    fecharTranca(); 
    
    Serial.println("========================================");
    if (modoSistema == 0) {
      Serial.println(">>> MODO: ACESSO NORMAL <<<");
      displayMsg("MODO TRABALHO", "Aproxime a tag..");
    }
    else if (modoSistema == 1) {
      Serial.println(">>> MODO: ADMIN OPERADORES <<<");
      displayMsg("MODO ADMIN", "OPERADORES");
    }
    else if (modoSistema == 2) {
      Serial.println(">>> MODO: ADMIN FERRAMENTAS <<<");
      displayMsg("MODO ADMIN", "FERRAMENTAS");
    }
    Serial.println("========================================");
    delay(2000);
  } 
  // EXECUÇÃO DO FLUXO SEGUNDO O MODO ATUAL
  else {
    if (modoSistema == 0) {
      fluxoNormal(idLida);
    } else {
      fluxoAdmin(idLida, (modoSistema == 1 ? "OPERADOR" : "FERRAMENTA"));
    }
  }

  // Interrompe a comunicação com a tag atual para evitar leituras duplicadas
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(500); 
}