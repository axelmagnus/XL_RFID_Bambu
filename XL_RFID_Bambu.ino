// Minimal RC522 + ESP8266 (Feather HUZZAH) reader for Bambu Lab filament tags
// Uses hardware SPI and reads MIFARE Classic data when provided per-sector keys
// derived with https://github.com/Bambu-Research-Group/RFID-Tag-Guide (deriveKeys.py).

#include <SPI.h>
#include <MFRC522.h>

// Pin mapping (ESP8266 Arduino numbers, not Dx labels):
// SS/SDA  -> 16 (GPIO16, usually labeled D0) // user-chosen; OK as chip-select
// RST     -> 2  (GPIO2,  usually labeled D4) // must stay HIGH at boot; fine for RC522 reset input
// SCK     -> 14 (GPIO14, D5)
// MOSI    -> 13 (GPIO13, D7)
// MISO    -> 12 (GPIO12, D6)
// 3.3V and GND only; do NOT feed the RC522 with 5V.

static constexpr uint8_t SS_PIN = 16; // SDA/SS moved to GPIO16 (D0)
static constexpr uint8_t RST_PIN = 2; // RC522 RST on GPIO2 (D4)

MFRC522 rfid(SS_PIN, RST_PIN);

// Paste the 16 derived A-keys (one per sector, 6 bytes each) after running:
//   python3 deriveKeys.py <UID-hex-no-spaces>
// from https://github.com/Bambu-Research-Group/RFID-Tag-Guide.
// Placeholder zeros mean "not set"; the sketch will remind you over Serial.
static byte SECTOR_KEY_A[16][6] = {
    {0, 0, 0, 0, 0, 0}, // sector  0
    {0, 0, 0, 0, 0, 0}, // sector  1
    {0, 0, 0, 0, 0, 0}, // sector  2
    {0, 0, 0, 0, 0, 0}, // sector  3
    {0, 0, 0, 0, 0, 0}, // sector  4
    {0, 0, 0, 0, 0, 0}, // sector  5
    {0, 0, 0, 0, 0, 0}, // sector  6
    {0, 0, 0, 0, 0, 0}, // sector  7
    {0, 0, 0, 0, 0, 0}, // sector  8
    {0, 0, 0, 0, 0, 0}, // sector  9
    {0, 0, 0, 0, 0, 0}, // sector 10
    {0, 0, 0, 0, 0, 0}, // sector 11
    {0, 0, 0, 0, 0, 0}, // sector 12
    {0, 0, 0, 0, 0, 0}, // sector 13
    {0, 0, 0, 0, 0, 0}, // sector 14
    {0, 0, 0, 0, 0, 0}  // sector 15
};

static void printHex(byte *buffer, byte bufferSize)
{
    for (byte i = 0; i < bufferSize; i++)
    {
        if (buffer[i] < 0x10)
            Serial.print('0');
        Serial.print(buffer[i], HEX);
        if (i + 1 < bufferSize)
            Serial.print(' ');
    }
}

static bool keysAreFilled()
{
    for (uint8_t s = 0; s < 16; s++)
    {
        bool any = false;
        for (uint8_t b = 0; b < 6; b++)
        {
            if (SECTOR_KEY_A[s][b] != 0)
            {
                any = true;
                break;
            }
        }
        if (!any)
            return false;
    }
    return true;
}

static uint16_t le16(const byte *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static float leFloat(const byte *p)
{
    float f = 0.0f;
    memcpy(&f, p, sizeof(float));
    return f;
}

static void decodeKnownBlock(uint8_t block, const byte *data)
{
    switch (block)
    {
    case 1: // Material IDs
        Serial.print("Block 1 (Material variant / ID): ");
        for (int i = 0; i < 8; i++)
            Serial.write(data[i]);
        Serial.print(" / ");
        for (int i = 8; i < 16; i++)
            Serial.write(data[i]);
        Serial.println();
        break;
    case 2: // Filament type
        Serial.print("Block 2 (Filament type): ");
        for (int i = 0; i < 16; i++)
            Serial.write(data[i]);
        Serial.println();
        break;
    case 5: // Color/weight/diameter
        Serial.print("Block 5 Color RGBA: 0x");
        for (int i = 3; i >= 0; i--)
        {
            if (data[i] < 0x10)
                Serial.print('0');
            Serial.print(data[i], HEX);
        }
        Serial.print("  Weight(g): ");
        Serial.print(le16(&data[4]));
        Serial.print("  Diameter(mm): ");
        Serial.println(leFloat(&data[8]), 3);
        break;
    case 6: // Temps
        Serial.print("Block 6 DryTemp: ");
        Serial.print(le16(&data[0]));
        Serial.print("C  DryTime(h): ");
        Serial.print(le16(&data[2]));
        Serial.print("  BedTemp: ");
        Serial.print(le16(&data[6]));
        Serial.print("C  HotendMax: ");
        Serial.print(le16(&data[8]));
        Serial.print("C  HotendMin: ");
        Serial.println(le16(&data[10]));
        break;
    case 8: // Nozzle diameter (LE float at offset 12)
        Serial.print("Block 8 Nozzle(mm): ");
        Serial.println(leFloat(&data[12]), 3);
        break;
    case 9: // Tray UID
        Serial.print("Block 9 (Tray UID): ");
        for (int i = 0; i < 16; i++)
            Serial.write(data[i]);
        Serial.println();
        break;
    case 10: // Spool width *100 at offset 4
        Serial.print("Block 10 Spool width(mm): ");
        Serial.println(le16(&data[4]) / 100.0f, 2);
        break;
    case 12: // Production date/time string
        Serial.print("Block 12 Prod date: ");
        for (int i = 0; i < 16; i++)
            Serial.write(data[i]);
        Serial.println();
        break;
    case 14: // Filament length meters at offset 4
        Serial.print("Block 14 Length(m): ");
        Serial.println(le16(&data[4]));
        break;
    case 16: // Extra color info
        Serial.print("Block 16 FormatId: ");
        Serial.print(le16(&data[0]));
        Serial.print("  ColorCount: ");
        Serial.print(le16(&data[2]));
        Serial.print("  SecondColor ABGR: 0x");
        for (int i = 7; i >= 4; i--)
        {
            if (data[i] < 0x10)
                Serial.print('0');
            Serial.print(data[i], HEX);
        }
        Serial.println();
        break;
    default:
        break;
    }
}

static void readClassic()
{
    MFRC522::MIFARE_Key key;
    byte buffer[18];
    byte size = sizeof(buffer);

    for (uint8_t sector = 0; sector < 16; sector++)
    {
        memcpy(key.keyByte, SECTOR_KEY_A[sector], 6);

        for (uint8_t b = 0; b < 3; b++) // skip trailer blocks (every 4th block)
        {
            uint8_t block = sector * 4 + b;

            MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
            if (status != MFRC522::STATUS_OK)
            {
                Serial.print("Auth fail sector ");
                Serial.print(sector);
                Serial.print(" block ");
                Serial.print(block);
                Serial.print(": ");
                Serial.println(rfid.GetStatusCodeName(status));
                continue;
            }

            size = sizeof(buffer);
            status = rfid.MIFARE_Read(block, buffer, &size);
            if (status != MFRC522::STATUS_OK)
            {
                Serial.print("Read fail block ");
                Serial.print(block);
                Serial.print(": ");
                Serial.println(rfid.GetStatusCodeName(status));
                continue;
            }

            Serial.print("Block ");
            Serial.print(block);
            Serial.print(" : ");
            printHex(buffer, 16);
            Serial.println();
            decodeKnownBlock(block, buffer);
        }
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
    }

    SPI.begin();
    rfid.PCD_Init();

    Serial.println("RC522 ready. Present a Bambu Lab spool/tag...");
    rfid.PCD_DumpVersionToSerial();

    if (!keysAreFilled())
    {
        Serial.println("Keys not set. Run deriveKeys.py with the tag UID and paste 16 lines (6-byte hex) into SECTOR_KEY_A.");
    }
}

void loop()
{
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    {
        delay(50);
        return;
    }

    Serial.println();
    Serial.print("Tag UID: ");
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();

    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    Serial.print("Type: ");
    Serial.println(rfid.PICC_GetTypeName(piccType));

    if (!keysAreFilled())
    {
        Serial.println("Add keys to SECTOR_KEY_A to read data blocks.");
    }
    else
    {
        readClassic();
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    Serial.println("-----");
    delay(200);
}
