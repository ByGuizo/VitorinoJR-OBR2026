// Feito por ByGuizo

const int E_RPWM = 5;
const int E_LPWM = 6;
const int D_RPWM = 9;
const int D_LPWM = 10;

const int PINO_ESQ = A0;
const int PINO_CEN = A1;
const int PINO_DIR = A2;

const int TRIG = 11;
const int ECHO = 12;

const int BRANCO_REF    =  60;
const int PRETO_REF     = 290;
const int LIMIAR        = 175;
const int LIMIAR_ALERTA = 120;

const int   VEL_RETO     = 120;
const int   VEL_CURVA    =  45;
const float KP           = 220.0;
const float KD           = 400.0;
const int   GIRO_MAX_MOV =  90;
const unsigned long PERIODO_D = 10;

const float POS_PARA   = 0.20;
const float POS_SAI    = 0.05;
const float POS_DERIVA = 0.06;

const int GIRO_PIVO_MIN =  95;
const int GIRO_PIVO_MAX = 170;

const int FREIO_PWM      = 120;
const int FREIO_MS       =  25;
const int FREIO_GIRO_PWM = 110;
const int FREIO_GIRO_MS  =  20;

const int GIRO_BUSCA = 120;
const unsigned long TEMPO_BUSCA = 500;
const unsigned long JANELA_GAP  = 1000;
const float POS_GAP  = 0.05;
const int VEL_GAP    = VEL_RETO;
const int AJUSTE_GAP = 0;

#define USAR_ULTRASSONICO 0

const int LADO_DESVIO = 2;

const int DIST_OBSTACULO = 10;
const int DIST_MAX_CM    = 60;
const unsigned long INTERVALO_ULTRA = 60;
const int CONFIRMACOES   = 2;
const unsigned long TIMEOUT_ECHO_US = 12000UL;
const unsigned long TEMPO_IGNORA_POS_DESVIO = 1500;

const int VEL_DESVIO  = 130;
const int GIRO_90_PWM = 150;
const unsigned long TEMPO_GIRO_90_ESQ = 1200;
const unsigned long TEMPO_GIRO_90_DIR = 1200;

const int AJUSTE_GIRO_SAIR       = 0;
const int AJUSTE_GIRO_ENDIREITAR = 120;
const int AJUSTE_GIRO_LINHA      = 120;

const unsigned long TEMPO_RE       = 150;
const unsigned long TEMPO_LATERAL  = 800;
const unsigned long TEMPO_PASSAR   = 1480;
const unsigned long PAUSA_MS       = 120;

const int VEL_ATE_LINHA = 100;
const unsigned long TEMPO_MAX_ATE_LINHA = 3000;
const unsigned long TEMPO_CRUZAR_LINHA  = 150;
const int GIRO_ALINHA = 120;
const unsigned long TEMPO_MIN_ALINHA = 150;
const unsigned long TEMPO_MAX_ALINHA = 1500;

const unsigned long DELAY_INICIAL = 1000;
#define DEBUG_SERIAL 1

bool pivotando = false;
bool semLinha  = false;
bool modoGap   = false;
int  ultimoLado = 1;
int  ultimaBase = 0;
int  ultimoGiro = 0;
unsigned long saidaDaLinha = 0;
unsigned long tAlinhado    = 0;
unsigned long tLateral     = 0;

float posFilt = 0, posAnt = 0, dpos = 0;
unsigned long tD = 0;

int   contadorObst = 0;
float ultimaDist   = -1;
unsigned long proximaLeituraUltra = 0;
unsigned long fimIgnoraObst       = 0;

unsigned long proximaImpressao = 0;
const int INTERVALO_IMPRESSAO = 200;

void ladoEsq(int vel) {
  vel = constrain(vel, -255, 255);
  if (vel >= 0) { analogWrite(E_LPWM, 0); analogWrite(E_RPWM, vel);  }
  else          { analogWrite(E_RPWM, 0); analogWrite(E_LPWM, -vel); }
}
void ladoDir(int vel) {
  vel = constrain(vel, -255, 255);
  if (vel >= 0) { analogWrite(D_LPWM, 0); analogWrite(D_RPWM, vel);  }
  else          { analogWrite(D_RPWM, 0); analogWrite(D_LPWM, -vel); }
}
void mover(int base, int giro) {
  ladoEsq(base + giro);
  ladoDir(base - giro);
}
void parar() { ladoEsq(0); ladoDir(0); }

void freioFrente() {
  if (FREIO_PWM <= 0) return;
  ladoEsq(-FREIO_PWM); ladoDir(-FREIO_PWM);
  delay(FREIO_MS);
}
void freioGiro(int ladoGiro) {
  if (FREIO_GIRO_PWM <= 0) return;
  mover(0, -ladoGiro * FREIO_GIRO_PWM);
  delay(FREIO_GIRO_MS);
}

uint8_t nivel(int v) {
  if (v > LIMIAR)        return 2;
  if (v > LIMIAR_ALERTA) return 1;
  return 0;
}
float normal(int v) {
  float n = (float)(v - BRANCO_REF) / (float)(PRETO_REF - BRANCO_REF);
  if (n < 0) n = 0;
  if (n > 1) n = 1;
  return n;
}
bool algumSensorNoPreto() {
  return nivel(analogRead(PINO_ESQ)) == 2 ||
         nivel(analogRead(PINO_CEN)) == 2 ||
         nivel(analogRead(PINO_DIR)) == 2;
}

float lerDistanciaCM() {
  if (digitalRead(ECHO) == HIGH) return -2;

  digitalWrite(TRIG, LOW);  delayMicroseconds(4);
  digitalWrite(TRIG, HIGH); delayMicroseconds(12);
  digitalWrite(TRIG, LOW);

  unsigned long dur = pulseIn(ECHO, HIGH, TIMEOUT_ECHO_US);
  if (dur == 0) return -1;

  float cm = dur / 58.0;
  if (cm < 2 || cm > DIST_MAX_CM) return -1;
  return cm;
}

bool obstaculoDetectado(unsigned long agora, bool podeDesviar) {
  if ((long)(agora - proximaLeituraUltra) < 0) return false;
  proximaLeituraUltra = agora + INTERVALO_ULTRA;

  ultimaDist = lerDistanciaCM();

  if (!podeDesviar) { contadorObst = 0; return false; }

  if (ultimaDist > 0 && ultimaDist <= DIST_OBSTACULO) contadorObst++;
  else                                                contadorObst = 0;

  return contadorObst >= CONFIRMACOES;
}

void girar90(int lado, int ajusteMs) {
  unsigned long tBase = (lado < 0) ? TEMPO_GIRO_90_ESQ : TEMPO_GIRO_90_DIR;
  long t = (long)tBase + ajusteMs;
  if (t < 0) t = 0;
  mover(0, lado * GIRO_90_PWM);
  delay(t);
  freioGiro(lado);
  parar();
  delay(PAUSA_MS);
}

void andarReto(unsigned long ms) {
  mover(VEL_DESVIO, AJUSTE_GAP);
  delay(ms);
  freioFrente();
  parar();
  delay(PAUSA_MS);
}

bool andarAteLinha(unsigned long timeout) {
  unsigned long t0 = millis();
  mover(VEL_ATE_LINHA, AJUSTE_GAP);
  while (millis() - t0 < timeout) {
    if (algumSensorNoPreto()) return true;
  }
  parar();
  return false;
}

void alinharNaLinha(int lado) {
  unsigned long t0 = millis();
  mover(0, lado * GIRO_ALINHA);
  while (millis() - t0 < TEMPO_MAX_ALINHA) {
    if (millis() - t0 > TEMPO_MIN_ALINHA &&
        nivel(analogRead(PINO_CEN)) == 2) break;
  }
  freioGiro(lado);
  parar();
  delay(PAUSA_MS);
}

void desfazerBusca(int lado, unsigned long ms) {
  unsigned long t0 = millis();
  mover(0, -lado * GIRO_BUSCA);
  while (millis() - t0 < ms) {
    if (algumSensorNoPreto()) break;
  }
  freioGiro(-lado);
  parar();
  delay(PAUSA_MS);
}

void desviarObstaculo() {
  int lado = (LADO_DESVIO == 1) ? -1 : 1;

#if DEBUG_SERIAL
  Serial.print(F(">> OBSTACULO a "));
  Serial.print(ultimaDist);
  Serial.println(lado < 0 ? F(" cm - desviando pela ESQUERDA")
                          : F(" cm - desviando pela DIREITA"));
#endif

  freioFrente();
  parar();
  delay(PAUSA_MS);
  if (TEMPO_RE > 0) {
    mover(-VEL_DESVIO, 0);
    delay(TEMPO_RE);
    parar();
    delay(PAUSA_MS);
  }

  girar90(lado,  AJUSTE_GIRO_SAIR);
  andarReto(TEMPO_LATERAL);
  girar90(-lado, AJUSTE_GIRO_ENDIREITAR);
  andarReto(TEMPO_PASSAR);
  girar90(-lado, AJUSTE_GIRO_LINHA);

  bool achou = andarAteLinha(TEMPO_MAX_ATE_LINHA);

  if (achou) {
    mover(VEL_ATE_LINHA, AJUSTE_GAP);
    delay(TEMPO_CRUZAR_LINHA);
    freioFrente();
    parar();
    delay(PAUSA_MS);

    alinharNaLinha(lado);
  }

#if DEBUG_SERIAL
  Serial.println(achou ? F(">> desvio concluido, voltando a seguir a linha")
                       : F(">> linha nao encontrada no desvio"));
#endif

  pivotando  = false;
  semLinha   = false;
  modoGap    = false;
  ultimoLado = lado;
  ultimaBase = 0;
  ultimoGiro = 0;
  posFilt = posAnt = dpos = 0;
  tAlinhado  = millis();
  tLateral   = 0;
  contadorObst  = 0;
  fimIgnoraObst = millis() + TEMPO_IGNORA_POS_DESVIO;
}

void setup() {
  pinMode(E_RPWM, OUTPUT); pinMode(E_LPWM, OUTPUT);
  pinMode(D_RPWM, OUTPUT); pinMode(D_LPWM, OUTPUT);
  pinMode(TRIG, OUTPUT);   pinMode(ECHO, INPUT);
  digitalWrite(TRIG, LOW);

  Serial.begin(115200);
  parar();
  delay(DELAY_INICIAL);
  tAlinhado = millis();

  Serial.println(F("ESQ     CEN     DIR     pos   base giro  dist  acao"));
}

void loop() {
  unsigned long agora = millis();

#if USAR_ULTRASSONICO
  bool podeDesviar = !pivotando && (long)(agora - fimIgnoraObst) >= 0;
  if (obstaculoDetectado(agora, podeDesviar)) {
    desviarObstaculo();
    return;
  }
#endif

  int vEsq = analogRead(PINO_ESQ);
  int vCen = analogRead(PINO_CEN);
  int vDir = analogRead(PINO_DIR);

  uint8_t lE = nivel(vEsq);
  uint8_t lC = nivel(vCen);
  uint8_t lD = nivel(vDir);

  float nE = normal(vEsq);
  float nC = normal(vCen);
  float nD = normal(vDir);

  const char* acao = "";
  int base = 0, giro = 0;
  float pos = 0;

  bool viuLinha = (lE > 0 || lC > 0 || lD > 0);

  if (viuLinha) {
    semLinha = false;
    modoGap  = false;

    float soma = nE + nC + nD;
    if (soma > 0.02) pos = (nD - nE) / soma;
    float absPos = fabs(pos);

    if (soma > 0.5) {
      posFilt = 0.7 * posFilt + 0.3 * pos;
      if (agora - tD >= PERIODO_D) {
        dpos = posFilt - posAnt;
        posAnt = posFilt;
        tD = agora;
      }
    }

    if (lE > 0 || lD > 0) tLateral = agora;

    if      (lD > lE) ultimoLado =  1;
    else if (lE > lD) ultimoLado = -1;
    else if (lC == 2 && absPos > POS_DERIVA) ultimoLado = (pos > 0) ? 1 : -1;

    if (lE == 2 && lC == 2 && lD == 2) {
      pivotando = false;
      base = VEL_RETO; giro = 0;
      acao = "cruzamento";
    }
    else if (!pivotando && lC == 1 && lE == 0 && lD == 0 &&
             fabs(posFilt) < POS_GAP) {
      base = VEL_RETO; giro = 0;
      tAlinhado = agora;
      acao = "centro na borda (reto)";
    }
    else {
      bool precisaPivo = (lC < 2) || (absPos > POS_PARA);
      bool alinhado    = (lC == 2) && (absPos < POS_SAI);

      if (!pivotando && precisaPivo) {
        if (ultimaBase > 60) freioFrente();
        pivotando = true;
      }
      else if (pivotando && alinhado) {
        if (ultimoGiro != 0) freioGiro(ultimoGiro > 0 ? 1 : -1);
        pivotando = false;
      }

      if (pivotando) {
        int lado;
        if      (lD > lE) lado =  1;
        else if (lE > lD) lado = -1;
        else if (lC == 2 && absPos > POS_DERIVA) lado = (pos > 0) ? 1 : -1;
        else    lado = ultimoLado;

        float intens = absPos;
        if (intens > 1) intens = 1;
        base = 0;
        giro = lado * (GIRO_PIVO_MIN + (int)((GIRO_PIVO_MAX - GIRO_PIVO_MIN) * intens));
        acao = (lado > 0) ? "PIVO dir (no eixo)" : "PIVO esq (no eixo)";
      }
      else {
        float f = absPos / POS_PARA;
        if (f > 1) f = 1;
        base = VEL_RETO - (int)((VEL_RETO - VEL_CURVA) * f);
        giro = constrain((int)(KP * pos + KD * dpos), -GIRO_MAX_MOV, GIRO_MAX_MOV);
        acao = (absPos < POS_SAI) ? "reto" : "ajuste andando";

        if (lC == 2 && lE == 0 && lD == 0) tAlinhado = agora;
      }
    }
  }
  else {
    if (!semLinha) {
      semLinha = true;
      saidaDaLinha = agora;
      modoGap = !pivotando &&
                (agora - tAlinhado < JANELA_GAP) &&
                (tLateral <= tAlinhado);
      if (!modoGap) {
        if (!pivotando && ultimaBase > 60) freioFrente();
        pivotando = true;
      }
    }

    unsigned long fora = agora - saidaDaLinha;
    if (!modoGap && fora >= TEMPO_BUSCA) {
      desfazerBusca(ultimoLado, TEMPO_BUSCA);
      modoGap = true;
      saidaDaLinha = millis();
    }

    if (modoGap) {
      pivotando = false;
      base = VEL_GAP;
      giro = AJUSTE_GAP;
      acao = "GAP - reto";
    }
    else {
      base = 0;
      giro = GIRO_BUSCA * ultimoLado;
      acao = "buscando linha (no eixo)";
    }
  }

  mover(base, giro);
  ultimaBase = base;
  ultimoGiro = giro;

#if DEBUG_SERIAL
  if ((long)(agora - proximaImpressao) >= 0) {
    proximaImpressao = agora + INTERVALO_IMPRESSAO;
    Serial.print(vEsq); Serial.print(lE == 2 ? F("P") : (lE == 1 ? F("b") : F(".")));
    Serial.print(F("   "));
    Serial.print(vCen); Serial.print(lC == 2 ? F("P") : (lC == 1 ? F("b") : F(".")));
    Serial.print(F("   "));
    Serial.print(vDir); Serial.print(lD == 2 ? F("P") : (lD == 1 ? F("b") : F(".")));
    Serial.print(F("   "));
    Serial.print((int)(pos * 100)); Serial.print(F("   "));
    Serial.print(base); Serial.print(F("  "));
    Serial.print(giro); Serial.print(F("  "));
    Serial.print((int)ultimaDist); Serial.print(F("  "));
    Serial.println(acao);
  }
#endif
}