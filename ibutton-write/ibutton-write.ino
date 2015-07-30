/*
  based on "Копировщик iButton ключей, ATmega8 + дисплей Nokia 3310: iButton-Mega8-3310.rar RW1990.1, RW1990.2, TM2004" (C)Serg22
  http://serg22.sibgtu.ru/radio/
*/

#include <OneWire.h>

// iButton pin.
#define PIN 10

// menu navigation pins
#define PIN_BUTTON_UP 7
#define PIN_BUTTON_DOWN 6
#define PIN_BUTTON_ESC 5
#define PIN_BUTTON_ENTER 4


#define _UP             1
#define _DOWN           2
#define _ESC            3
#define _ENTER          5

#define _FALSE          0
#define _TRUE           1

#define MODE_MENU           0
#define MODE_READ           1
#define MODE_VERIFY         2
#define MODE_TYPE_KEY       3
#define MODE_WRITE_TM2004   4
#define MODE_WRITE_RW1990_2 5
#define MODE_WRITE_RW1990_1 6
#define MODE_WRITE_RW1990   7
#define MODE_WRITE_TM08v2   8
#define MODE_WRITE_AUTO     9

#define TYPE_KEY_DS1990     0
#define TYPE_KEY_RW1990_1   1
#define TYPE_KEY_RW1990_2   2
#define TYPE_KEY_TM2004     3

OneWire  ds(PIN);

char * str_read1_onewire    = "Waiting for a key... "; //Жду ключ...   ";
char * str_read2_onewire    = "Read the key "; //Прочитан ключ ";
char * str_read3_onewire    = "Key in memory "; //Ключ в памяти ";
char * str_read4_onewire    = "Validated OK "; //Код идентичен ";
char * str_read5_onewire    = "Validation FAILED "; //"Код с ошибкой ";
char * str_typekey_onewire  = "Key Type: "; //Тип ключа:    ";
char * str_type_tm2004      = "TM2004        ";
char * str_type_rw1990_1    = "RW1990.1      ";
char * str_type_rw1990_2    = "RW1990.2      ";
char * str_write_auto     = "Write type Auto"; //сохр.Автоматич";
char * str_write_tm2004   = "Write TM2004"; //сохр.TM2004   ";
char * str_write_rw1990_2 = "Write RW1990.2"; //сохр.RW1990.2 ";
char * str_write_rw1990_1 = "Write RW1990.1"; //сохр.RW1990.1 ";


uint8_t pin;
uint8_t port;
uint8_t bitmask;
volatile uint8_t *outputReg;
volatile uint8_t *inputReg;
volatile uint8_t *modeReg;

int mode;
uint8_t rom_code[9];
uint8_t rom_code_saved[9];
uint8_t rom_code_verify[9];



//тип структуры для связанных спискоффф.
typedef struct MenuElem {
    char            *str;     //описание пункта меню
    struct MenuElem *prev;    //предыдущий пункт меню
    struct MenuElem *next;    //следующий пункт меню
    struct MenuElem *sel;     //пункт меню, на который осуществляется переход по нажатию <ENTER>
    struct MenuElem *ret;     //пункт меню, на который осуществляется возврат по нажатию <ESC>
    struct MenuElem *first;   //начальный пункт меню
    void (*func)(void);       //выполняемая процедура
} MENU;


extern MENU M11,M12,M13,M14,\
                  M21,M22,M23,M24,M25,\
                  M111,M131,M141,M211,M221,M241,M251;

MENU *CurrentElem = &M11; //текущий (первый) элемент меню

//-------- *str ------ *prev *next  *sel  *ret *first *func-----------------
MENU M11 = {"Read",    &M14, &M12, &M111, &M11, &M11, NULL};
MENU M12 = {"Write",   &M11, &M13, &M21,  &M12, &M11, NULL};
MENU M13 = {"Verify",  &M12, &M14, &M131, &M13, &M11, NULL};
MENU M14 = {"Type",    &M13, &M11, &M141, &M14, &M11, NULL};
//--------------------------------------------------------------------------
MENU M21 = {"Auto",     &M25, &M22, &M211, &M11, &M21, NULL};
MENU M22 = {"TM2004",   &M21, &M23, &M221, &M11, &M21, NULL};
MENU M23 = {"RW1990",   &M22, &M24, NULL,  &M11, &M21, NULL};
MENU M24 = {"RW1990.1", &M23, &M25, &M241, &M11, &M21, NULL};
MENU M25 = {"RW1990.2", &M24, &M21, &M251, &M11, &M21, NULL};
//--------------------------------------------------------------------------
void startReadOnewire(void);
void startVerifyOnewire(void);
void startTypeKey(void);
void startWriteTM2004(void);
void startWriteRW1990_1(void);
void startWriteRW1990_2(void);
void startWriteAuto(void);
//если необходимо, чтобы по переходу в определённый пункт меню выполнялась функция, то пишем:
MENU M111 = {NULL, NULL, NULL, NULL, &M11, NULL, startReadOnewire};
MENU M131 = {NULL, NULL, NULL, NULL, &M13, NULL, startVerifyOnewire};
MENU M141 = {NULL, NULL, NULL, NULL, &M14, NULL, startTypeKey};
MENU M211 = {NULL, NULL, NULL, NULL, &M21, NULL, startWriteAuto};
MENU M221 = {NULL, NULL, NULL, NULL, &M22, NULL, startWriteTM2004};
MENU M231 = {NULL, NULL, NULL, NULL, &M23, NULL, NULL};
MENU M241 = {NULL, NULL, NULL, NULL, &M24, NULL, startWriteRW1990_1};
MENU M251 = {NULL, NULL, NULL, NULL, &M25, NULL, startWriteRW1990_2};
//--------------------------------------------------------------------------



// = output ================================================================

void nokia_cls(void) { Serial.println("----------------------------------------"); }
void nokia_gotoxy(uint8_t x, uint8_t y) { Serial.println(); }
void nokia_putchar(char ch) { Serial.print(ch); }
const char * HEX_DIGITS = "0123456789abcdef";
void nokia_puthex(uint8_t i) { Serial.print(HEX_DIGITS[(i & 0x0f0)>>4]); Serial.print(HEX_DIGITS[i & 0x00f]); };

void lcdPutStringFLASH(char *str, uint8_t inv) {
    if(inv) nokia_putchar('[');
    while(*str) nokia_putchar(*str++);
    if(inv) nokia_putchar(']');
}

void lcdPutKey() {
    uint8_t i;
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_read2_onewire, _FALSE);
    i = 7;
    do {
        i--;
        nokia_puthex(rom_code_saved[i]);
    } while (i > 0);
}

// = menu =======================================================

void setupButtons() {
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_ESC, INPUT_PULLUP);
  pinMode(PIN_BUTTON_ENTER, INPUT_PULLUP);
}

char checkButton() {
  char pressed_key = 0;
  if ((digitalRead(PIN_BUTTON_UP) == LOW)||(digitalRead(PIN_BUTTON_DOWN) == LOW)||(digitalRead(PIN_BUTTON_ESC) == LOW)||(digitalRead(PIN_BUTTON_ENTER) == LOW)) {
      while (1) {
          delay(50);
          // save the last pressed button
          if (digitalRead(PIN_BUTTON_UP) == LOW) { pressed_key = _UP; }
          if (digitalRead(PIN_BUTTON_DOWN) == LOW) { pressed_key = _DOWN; }
          if (digitalRead(PIN_BUTTON_ESC) == LOW) { pressed_key = _ESC; }
          if (digitalRead(PIN_BUTTON_ENTER) == LOW) { pressed_key = _ENTER; }

          // return when all buttons are released
          if ((digitalRead(PIN_BUTTON_UP) == HIGH)&&(digitalRead(PIN_BUTTON_DOWN) == HIGH)&&(digitalRead(PIN_BUTTON_ESC) == HIGH)&&(digitalRead(PIN_BUTTON_ENTER) == HIGH)) {
              return pressed_key;
          }
      }
  } else return 0;
}

//процедура опроса кнопок
void keyScan(void) {
    uint8_t key = checkButton();//процедура опроса кнопок
    if (key) {
        switch (key) {
            //---------------------------------------------------------------------------
            case (_DOWN): {
                //перейти на следующий пункт меню
                if (CurrentElem -> next) CurrentElem = CurrentElem -> next;
            } break;
            //---------------------------------------------------------------------------
            case (_UP): {
                //перейти на предыдущий пункт меню
                if (CurrentElem -> prev) CurrentElem = CurrentElem -> prev;
            } break;
            //---------------------------------------------------------------------------
            case (_ENTER): {
                if (CurrentElem -> sel)    //перейти "вниз по дереву"
                {
                    nokia_cls();
                    CurrentElem = CurrentElem -> sel;
                }//end if
            } break;
            //---------------------------------------------------------------------------
            case (_ESC): {
                if (CurrentElem -> ret)     //перейти "вверх по дереву"
                {
                    nokia_cls();//очистить экран
                    CurrentElem = CurrentElem -> ret;
                }//end if
            } break;
            //---------------------------------------------------------------------------
        }//end switch
        //отобразить меню
        menuView();
    }//end if
}

//процедура прорисовки меню или запуска подпрограммы
void menuView(void) {
    uint8_t yy;
    MENU *pS;
    nokia_gotoxy(0, 0);
    // lcdPutStringFLASH(str_serg22, _FALSE);
    //если выбрана функция...
    if (CurrentElem -> func) CurrentElem -> func();//выполнить подпрограмму, если необхоодимо
    else
    {
        //иначе вывести пункты меню
        pS = CurrentElem -> first;
        yy = 1;//Y-координата начального пункта меню
        do {
            nokia_gotoxy(0, yy);//переходим на координаты
            if (pS == CurrentElem) lcdPutStringFLASH((pS -> str), _TRUE);//отображаем ИНВЕРСНО
            else lcdPutStringFLASH((pS -> str), _FALSE);//отображаем НЕИНВЕРСНО
            //след. пункт...
            pS = pS -> next;
            yy++;
        } while (pS != CurrentElem -> first);
    }//end else
}
//-----------------------------------------------------------------------------------------


void startMenu() {
  mode = MODE_MENU;
  if(CurrentElem -> ret) {
    CurrentElem = CurrentElem -> ret;
  }

  menuView();
}


// ----------------------------------------------------------------
uint8_t ReadOneWire() {
    uint8_t i;
    do {
        ds.write(0x33);
        for (i=0; i < 8; i++) {
                rom_code[i] = ds.read();
        }
    } while ((ds.reset() == 1) & (OneWire::crc8(rom_code,7) != rom_code[7] )) ;
    if (OneWire::crc8(rom_code,7) != rom_code[7] ) return 0; else return 1;
}

void verifyKey() {
    uint8_t i;
    uint8_t ident;

    ident = 1;
    for (i=1; i<8; i++) {
        if (rom_code_saved[i] != rom_code_verify[i]) {ident = 0;}
    }

    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_read3_onewire, _FALSE);
    i = 7;
    do {
        i--;
        nokia_puthex(rom_code_saved[i]);
    } while (i > 0);
    lcdPutStringFLASH(str_read2_onewire, _FALSE);
    i = 7;
    do {
        i--;
        nokia_puthex(rom_code_verify[i]);
    } while (i > 0);

    if (ident == 1) {
            lcdPutStringFLASH(str_read4_onewire, _FALSE);
    } else {
            lcdPutStringFLASH(str_read5_onewire, _FALSE);
    }

}

void startReadOnewire() {
    mode = MODE_READ;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_read1_onewire, _FALSE);
}

void startVerifyOnewire() {
    mode = MODE_VERIFY;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_read1_onewire, _FALSE);
}

void startTypeKey() {
    mode = MODE_TYPE_KEY;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_typekey_onewire, _FALSE);
}

void startWriteAuto() {
    mode = MODE_WRITE_AUTO;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_write_auto, _FALSE);
    rom_code_saved[7] = OneWire::crc8(rom_code_saved,7);
}

void startWriteTM2004() {
    mode = MODE_WRITE_TM2004;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_write_tm2004, _FALSE);
    rom_code_saved[7] = OneWire::crc8(rom_code_saved,7);
}

void startWriteRW1990_2() {
    mode = MODE_WRITE_RW1990_2;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_write_rw1990_2, _FALSE);
    rom_code_saved[7] = OneWire::crc8(rom_code_saved,7);
}

void startWriteRW1990_1() {
    mode = MODE_WRITE_RW1990_1;
    nokia_cls();
    nokia_gotoxy(0, 0);
    lcdPutStringFLASH(str_write_rw1990_1, _FALSE);
    rom_code_saved[7] = OneWire::crc8(rom_code_saved,7);
}

void programmPulse() {
    *outputReg |= bitmask;
    *modeReg |= bitmask;
    delayMicroseconds(600);
    *outputReg &= ~bitmask;
    delayMicroseconds(5);
    *outputReg |= bitmask;
    delay(50);
    *modeReg &= ~bitmask;
    *outputReg &= ~bitmask;
}

uint8_t writeOneWireTM2004() {
    uint8_t i, tmp;
    if (ds.reset() == 1) {
        ds.write(0x3C);
        ds.write(0x00);
        ds.write(0x00);
        for (i = 0; i < 8; i++){
            ds.write(rom_code_saved[i]);
            tmp = ds.read();
            programmPulse();
            tmp = ds.read();
        }
    }
    return 1;
}

void timeSlot(unsigned char data) {
    *modeReg |= bitmask;
    *outputReg &= ~bitmask;
    if (data) {
        delayMicroseconds(6);
    } else {
        delayMicroseconds(60);
    }
    *modeReg &= ~bitmask;
    delay(10);
}

void w1_WriteBit_save(unsigned char data) {
    delayMicroseconds(70);
    *modeReg |= bitmask;
    *outputReg &= ~bitmask;
    if (data) {
      delayMicroseconds(6);
    } else {
      delayMicroseconds(60);
    }
    *modeReg &= ~bitmask;
    delay(1);
}

void w1_save(unsigned char data) {
    unsigned char i;

    /* Send LSB first */
    for (i = 0; i < 8; i++) {
      w1_WriteBit_save(data & 0x01);
      data >>= 1;
    }
}

uint8_t typeKey() {
    uint8_t tmp;
    //Проверка на TM2004
    if (ds.reset() == 1) {
        ds.write(0xCC); //команда SkipROM
        ds.write(0xAA); //команда чтения по указанному адресу
        ds.write(0x00);
        ds.write(0x00);
        tmp = ds.read();
        if ( tmp == 0x9c ) {
             return TYPE_KEY_TM2004;
        }
    }

    //Проверка на RW1990.1
    if (ds.reset() == 1) {
        ds.write(0xD1); //Запрещаем запись
        timeSlot(1);
    }
    if (ds.reset() == 1) {
        ds.write(0xB5); //Проверяем состояние запреза записи
        tmp = ds.read();
        if ( tmp == 0xfe ) {
            if (ds.reset() == 1) {
                ds.write(0xD1); //Разрешаем запись
                timeSlot(0);
            }
            if (ds.reset() == 1) {
                ds.write(0xB5); //Проверяем состояние разрешения записи
                tmp = ds.read();
                if ( tmp == 0xff ) {
                    if (ds.reset() == 1) {
                        ds.write(0xD1); //Запрещаем запись
                        timeSlot(1);
                    }
                    return TYPE_KEY_RW1990_1;
                }
            }
        }
    }

    //Проверка на RW1990.2
    if (ds.reset() == 1) {
        ds.write(0x1D); //Разрешаем запись
        timeSlot(1);
    }
    if (ds.reset() == 1) {
        ds.write(0x1E); //Проверяем состояние запреза записи
        tmp = ds.read();
        if ( tmp == 0xfe ) {
            if (ds.reset() == 1) {
                ds.write(0x1D); //Запрещаем запись
                timeSlot(0);
            }
            return TYPE_KEY_RW1990_2;
        }
    }
}

uint8_t writeOneWireRW1990( uint8_t data ) {
    uint8_t i;
    switch (data) {
        case TYPE_KEY_RW1990_1: {
            if (ds.reset() == 1) {
                ds.write(0xD1); //Разрешаем запись
                timeSlot(0);
            }
            if (ds.reset() == 1) {
                ds.write(0xD5); //Команда записи
                for (i = 0; i < 8; i++){
                    w1_save(~rom_code_saved[i]);
                }
            }
            if (ds.reset() == 1) {
                ds.write(0xD1); //Запрещаем запись
                timeSlot(1);
            }

        } break;

        case TYPE_KEY_RW1990_2: {
            if (ds.reset() == 1) {
                ds.write(0x1D); //Разрешаем запись
                timeSlot(1);
            }
            if (ds.reset() == 1) {
                ds.write(0xD5); //Команда записи
                for (i = 0; i < 8; i++){
                        w1_save(rom_code_saved[i]);
                }
            }
            if (ds.reset() == 1) {
                ds.write(0x1D); //Запрещаем запись
                timeSlot(0);
            }
        } break;
    };

    return 1;
}


void setup(void) {
  Serial.begin(9600);

  pin = PIN;
  port = digitalPinToPort(pin);
  bitmask =  digitalPinToBitMask(pin);
  outputReg = portOutputRegister(port);
  inputReg = portInputRegister(port);
  modeReg = portModeRegister(port);

  setupButtons();
  menuView();
}


void loop(void) {
    uint8_t type_key;

    if (mode == MODE_MENU) {
      keyScan();
      if (mode == MODE_MENU) {
        return;
      }
    }

    if (ds.reset() == 1) {
        switch (mode) {
            case (MODE_READ): {
                if (ReadOneWire() == 1) {
                        memcpy(rom_code_saved, rom_code, 9);
                        lcdPutKey();
                        startMenu();
                }
            } break;

            case (MODE_VERIFY): {
                if (ReadOneWire() == 1) {
                        memcpy(rom_code_verify, rom_code, 9);
                        verifyKey();
                        startMenu();
                }
            } break;

            case (MODE_TYPE_KEY): {
                type_key = typeKey();
                switch (type_key) {
                    case TYPE_KEY_RW1990_1: {
                        nokia_gotoxy(0, 2);
                        lcdPutStringFLASH(str_type_rw1990_1, _FALSE);
                    } break;
                    case TYPE_KEY_RW1990_2: {
                        nokia_gotoxy(0, 2);
                        lcdPutStringFLASH(str_type_rw1990_2, _FALSE);
                    } break;
                    case TYPE_KEY_TM2004: {
                        nokia_gotoxy(0, 2);
                        lcdPutStringFLASH(str_type_tm2004, _FALSE);
                    } break;
                };
                startMenu();
            } break;

            case (MODE_WRITE_TM2004): {
                        if (writeOneWireTM2004() == 1) {
                                if (ds.reset() == 1) {
                                       if (ReadOneWire() == 1) {
                                                memcpy(rom_code_verify, rom_code, 9);
                                                verifyKey();
                                                startMenu();
                                        }
                                }
                        }
            } break;

            case (MODE_WRITE_RW1990_1): {
                        if (writeOneWireRW1990(TYPE_KEY_RW1990_1) == 1) {
                                if (ds.reset() == 1) {
                                       if (ReadOneWire() == 1) {
                                                memcpy(rom_code_verify, rom_code, 9);
                                                verifyKey();
                                                startMenu();
                                        }
                                }
                        }
            } break;

            case (MODE_WRITE_RW1990_2): {
                        if (writeOneWireRW1990(TYPE_KEY_RW1990_2) == 1) {
                                if (ds.reset() == 1) {
                                       if (ReadOneWire() == 1) {
                                                memcpy(rom_code_verify, rom_code, 9);
                                                verifyKey();
                                                startMenu();
                                        }
                                }
                        }
            } break;

            case (MODE_WRITE_AUTO): {
                type_key = typeKey();
                switch (type_key) {
                    case TYPE_KEY_RW1990_1: {
                        if (writeOneWireRW1990(TYPE_KEY_RW1990_1) == 1) {
                                if (ds.reset() == 1) {
                                       if (ReadOneWire() == 1) {
                                                memcpy(rom_code_verify, rom_code, 9);
                                                verifyKey();
                                                nokia_gotoxy(0, 5);
                                                lcdPutStringFLASH(str_type_rw1990_1, _FALSE);
                                                startMenu();
                                        }
                                }
                        }
                    } break;

                    case TYPE_KEY_RW1990_2: {
                        if (writeOneWireRW1990(TYPE_KEY_RW1990_2) == 1) {
                                if (ds.reset() == 1) {
                                       if (ReadOneWire() == 1) {
                                                memcpy(rom_code_verify, rom_code, 9);
                                                verifyKey();
                                                nokia_gotoxy(0, 5);
                                                lcdPutStringFLASH(str_type_rw1990_2, _FALSE);
                                                startMenu();
                                        }
                                }
                        }
                    } break;

                    case TYPE_KEY_TM2004: {
                        if (writeOneWireTM2004() == 1) {
                                if (ds.reset() == 1) {
                                       if (ReadOneWire() == 1) {
                                                memcpy(rom_code_verify, rom_code, 9);
                                                verifyKey();
                                                nokia_gotoxy(0, 5);
                                                lcdPutStringFLASH(str_type_tm2004, _FALSE);
                                                startMenu();
                                        }
                                }
                        }
                    } break;
                };
            } break;

        };
    }
}
