#include "stm32f4xx_conf.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_spi.h"
#include "ff.h"
#include "stdio.h"
#include "cstring.h"
#include "string.h"

typedef unsigned char   BYTE;
typedef unsigned long	DWORD;
static volatile BYTE Timer1, Timer2;
unsigned int d, f;

/* Definitions for MMC/SDC command */
#define CMD0    (0x40+0)    /* GO_IDLE_STATE */
#define CMD1    (0x40+1)    /* SEND_OP_COND */
#define CMD8    (0x40+8)    /* SEND_IF_COND */
#define CMD9    (0x40+9)    /* SEND_CSD */
#define CMD10    (0x40+10)    /* SEND_CID */
#define CMD12    (0x40+12)    /* STOP_TRANSMISSION */
#define CMD16    (0x40+16)    /* SET_BLOCKLEN */
#define CMD17    (0x40+17)    /* READ_SINGLE_BLOCK */
#define CMD18    (0x40+18)    /* READ_MULTIPLE_BLOCK */
#define CMD23    (0x40+23)    /* SET_BLOCK_COUNT */
#define CMD24    (0x40+24)    /* WRITE_BLOCK */
#define CMD25    (0x40+25)    /* WRITE_MULTIPLE_BLOCK */
#define CMD41    (0x40+41)    /* SEND_OP_COND (ACMD) */
#define CMD55    (0x40+55)    /* APP_CMD */
#define CMD58    (0x40+58)    /* READ_OCR */

BYTE buffer[4096];	// Bufor przechowuj¹cy kopiowane dane

// Funkcja znajduj¹ca pliki w folderze przekazanym w parametrze
FRESULT scan_files (
    char* path        /* Start node to be scanned (also used as work area) */
)
{
	FIL fs, fd;
	UINT bw, br;
    FRESULT res, fr;
    FILINFO fno;
    DIR dir;
    char * path_new;	// Œcie¿ka dla podfolderu
    char * pathToFile;	// Œcie¿ka do pliku, który ma zostaæ skopiowany
    char * pathToCopy;	// Œcie¿ka do miejsca gdzie ma zostaæ skopiowany plik
    char * dirCopy = "Kopia";	// Folder do którego kopiowane s¹ pliki
    char * private = "Private";
    char * newDir;	// Œcie¿ka folderu, który ma zostaæ utworzony na drugim urz¹dzeniu
    int i, j=0, n=0, m;
    char *fn;   /* This function is assuming non-Unicode cfg. */

#if _USE_LFN
    static char lfn[_MAX_LFN + 1];   /* Buffer to store the LFN */
    fno.lfname = lfn;
    fno.lfsize = sizeof lfn;
#endif

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */

            if (fno.fname[0] == '.') continue;             /* Ignore dot entry */

#if _USE_LFN
            fn = *fno.lfname ? fno.lfname : fno.fname;
#else
            fn = fno.fname;
#endif
            printf("fn: %s\n", fn);

            // Zapobiega wczytywaniu plików z folderu, do którego pliki s¹ kopiowane
            if (strcmp(fn, dirCopy) == 0) continue;
            // Zapobiega wczytywaniu plików z folderu Private
            if (strcmp(fn, private) == 0) continue;
            if (fno.fattrib & AM_DIR) {                    /* It is a directory */
            	GPIO_SetBits(GPIOD, GPIO_Pin_14);

            	// Zapisanie scie¿ki znalezionego folderu do zmiennej newDir
            	newDir = malloc(strlen(dirCopy)+strlen(path)+1+strlen(fn)+1);
            	strcpy(newDir, dirCopy);
            	strcat(newDir, path);
            	strcat(newDir, "/");
            	strcat(newDir, fn);

            	fr = f_mkdir(newDir);
            	free(newDir);

            	// Zapisanie Œcie¿ki podfolderu do zmiennej path_new
            	path_new = malloc(strlen(path)+strlen(fn)+1);
            	strcpy(path_new, path);
            	strcat(path_new, "/");
            	strcat(path_new, fn);

            	// Uruchomienie funkcji scan_files dla znalezionego folderu
                res = scan_files(path_new);
                free(path_new);
                if (res != FR_OK) break;
            } else {                                     /* It is a file. */
            	GPIO_SetBits(GPIOD, GPIO_Pin_13);
            	printf("%s/%s\n", path, fn);

            	// Zapisanie Œcie¿ki znalezionego pliku do zmiennej pathToFile
            	pathToFile = malloc(strlen(path)+1+strlen(fn)+1);
            	strcpy(pathToFile, path);
            	strcat(pathToFile, "/");
            	strcat(pathToFile, fn);
            	printf("pathToFile: %s\n", pathToFile);

            	// Zapisanie scie¿ki gdzie ma zostaæ skopiowany plik do zmiennej pathToCopy
            	pathToCopy = malloc(strlen(dirCopy)+strlen(path)+1+strlen(fn)+1);
            	strcpy(pathToCopy, dirCopy);
            	strcat(pathToCopy, path);
            	strcat(pathToCopy, "/");
            	strcat(pathToCopy, fn);
            	printf("pathToCopy: %s\n", pathToCopy);

            	// Otworzenie znalezionego pliku
                fr = f_open(&fs, pathToFile, FA_OPEN_EXISTING | FA_READ);
                // Utworzenie pliku na drugim urzadzeniu gdzie maja zostac skopiowane dane
                fr = f_open(&fd, pathToCopy, FA_CREATE_ALWAYS | FA_WRITE);
                free(pathToCopy);
                free(pathToFile);

                /* Kopiowanie danych
                 * ------------------------------------------------------
                 * Kopiowany powinien byæ tylko kawa³ek pliku. Plik err.file, pomimo tego, i¿
                 * wa¿y wiêcej ni¿ jest w stanie pomiesciæ bufor, kopiowany jest poprawnie.
                 * Natomiast podczas kopiowania pliku .txt, który jest wiêkszy ni¿ bufor,
                 * program siê zawiesza.
                 *
                 * funkcja f_lseek()?
                 * ------------------------------------------------------
                 */
                for (;;) {
                    fr = f_read(&fs, buffer, sizeof buffer, &br);  // Read a chunk of source file
                    if (fr || br == 0) break; // error or eof
                    fr = f_write(&fd, buffer, br, &bw);            // Write it to the destination file
                    if (fr || bw < br) break; // error or disk full
                }

                // Zamkniêcie plików
                f_close(&fs);
                f_close(&fd);

            	f++;
            }
        }
        // Zamkniêcie folderu
        f_closedir(&dir);
    }

    return res;
}

static
BYTE rcvr_spi() 		// Odebranie bajtu z SD
{
  BYTE Data = 0;

	while( !( SPI2->SR & SPI_SR_TXE ));
	SPI2->DR = 0xFF;
	while( !( SPI2->SR & SPI_SR_RXNE ));
	Data = SPI2->DR;

  return Data;
}

static
BYTE wait_ready (void)
{
    BYTE res;

    Timer2 = 50;    /* Wait for ready in timeout of 500ms */
    rcvr_spi();
    do
        res = rcvr_spi();
    while ((res != 0xFF) && Timer2);

    return res;
}

static
void xmit_spi (BYTE Data)  // Wyslanie bajtu do SD
{
	BYTE nought;
	while( !( SPI2->SR & SPI_SR_TXE ));
	SPI2->DR = Data;
	while( !( SPI2->SR & SPI_SR_RXNE ));
	nought = SPI2->DR;

}

static
BYTE send_cmd (
    BYTE cmd,        /* Command byte */
    DWORD arg        /* Argument */
)
{
    BYTE n, res;

    if (wait_ready() != 0xFF) return 0xFF;

    /* Send command packet */
    xmit_spi(cmd);                        /* Command */
    xmit_spi((BYTE)(arg >> 24));        /* Argument[31..24] */
    xmit_spi((BYTE)(arg >> 16));        /* Argument[23..16] */
    xmit_spi((BYTE)(arg >> 8));            /* Argument[15..8] */
    xmit_spi((BYTE)arg);                /* Argument[7..0] */
    n = 0;
    if (cmd == CMD0) n = 0x95;            /* CRC for CMD0(0) */
    if (cmd == CMD8) n = 0x87;            /* CRC for CMD8(0x1AA) */
    xmit_spi(n);

    /* Receive command response */
    if (cmd == CMD12)  rcvr_spi();        /* Skip a stuff byte when stop reading */
    n = 10;                                /* Wait for a valid response in timeout of 10 attempts */
    do
        res = rcvr_spi();
    while ((res & 0x80) && --n);

    return res;            /* Return with the response value */
}

int main(void)
{

	SystemInit();
	int i;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE); // zegar dla portu GPIO z którego wykorzystane zostan¹ piny do SPI (MOSI, MISO, SCK)
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE); // zegar dla portu GPIO z którego wykorzystany zostanie pin do SPI (CS)
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); // zegar dla modu³u SPI2
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure2;

	GPIO_InitStructure2.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13| GPIO_Pin_14| GPIO_Pin_15;
	GPIO_InitStructure2.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure2.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure2.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure2.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD, &GPIO_InitStructure2);


	//inicjalizacja pinów wykorzystywanych do SPI
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	// SPI SCK, MISO, MOSI
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	// konfigurcja pinu CS
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	// deselecy – ustawienie pinu w stan wysoki
	GPIO_SetBits(GPIOE, GPIO_Pin_3);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2); // SCK
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2); // MISO
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2); // MOSI

	// konfiguracja SPI w trybie MASTER
	SPI_InitTypeDef SPI_InitStructure;
	SPI_I2S_DeInit(SPI2);
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_Init(SPI2, &SPI_InitStructure);

	// Uruchomienie SPI2
	SPI_Cmd(SPI2, ENABLE);

	// Ustawienie pinu CS w stan niski
	GPIO_ResetBits(GPIOE, GPIO_Pin_3);

	// Wyslanie x10 danych 0xFF
	for(i=0; i<10; i++) {
		SPI_I2S_SendData(SPI2, 0xFF);
		while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
	}

	// Wyslanie komendy CMD0 do karty SD
	send_cmd(CMD0, 0);

	// Utworzenie sytemu plików fat
	FATFS fatfs;
    f_mount(0, &fatfs);
	/*
	FATFS fatfs;
	FIL fs, fd;
	FRESULT fr;
	UINT bw, br;

    f_mount(0, &fatfs);

    fr = f_open(&fs, "file.bin", FA_OPEN_EXISTING | FA_READ);
    fr = f_open(&fd, "/Nowy/file4.bin", FA_CREATE_ALWAYS | FA_WRITE);

    // Copy source to destination
    for (;;) {
        fr = f_read(&fs, buffer, sizeof buffer, &br);  // Read a chunk of source file
        if (fr || br == 0) break; // error or eof
        fr = f_write(&fd, buffer, br, &bw);            // Write it to the destination file
        if (fr || bw < br) break; // error or disk full
    }

    // Close open files
    f_close(&fs);
    f_close(&fd);
	*/

    scan_files("");

	GPIO_SetBits(GPIOD, GPIO_Pin_12);

	while(1);

}
