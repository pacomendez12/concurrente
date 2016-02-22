#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>

#define DIF 16

enum open_error {
	EOPEN =		-1,
	EHEADER =	-2,
	EBMP =		-3
};

#pragma pack(2) // Empaquetado de 2 bytes
typedef struct
{
    unsigned char magic1; // 'B'
    unsigned char magic2; // 'M'
    unsigned int size; // Tamaño
    unsigned short int reserved1, reserved2;
    unsigned int pixelOffset; // offset a la imagen
} HEADER;



#pragma pack() // Empaquetamiento por default
typedef struct
{
    unsigned int size; // Tamaño de este encabezado INFOHEADER
    int cols, rows; // Renglones y columnas de la imagen
    unsigned short int planes;
    unsigned short int bitsPerPixel; // Bits por pixel
    unsigned int compression;
    unsigned int cmpSize;
    int xScale, yScale;
    unsigned int numColors;
    unsigned int importantColors;
} INFOHEADER;

typedef struct
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} PIXEL;

typedef struct
{
    HEADER header;
    INFOHEADER infoheader;
    PIXEL *pixel;
} IMAGE;


struct thread_param{
	unsigned int start;
	unsigned int end;
	IMAGE *imagefte;
	IMAGE *imagedst;
};


//variables globales
IMAGE imagenfte,imagendst;
int n_threads;


int loadBMP(char *filename, IMAGE *image)
{
    FILE *fin;
    int i = 0;
    int totpixs = 0;
    fin = fopen(filename, "rb+");
	// Si el archivo no existe
    if (fin == NULL) {
		perror("Error: ");
        return EOPEN;
	}
	// Leer encabezado
    fread(&image->header, sizeof(HEADER), 1, fin);
	// Probar si es un archivo BMP
    if (!((image->header.magic1 == 'B') && (image->header.magic2 == 'M'))) {
		printf("Error reading header of image\n");
        return EHEADER;
	}
    fread(&image->infoheader, sizeof(INFOHEADER), 1, fin);
	// Probar si es un archivo BMP 24 bits no compactado
    if (!((image->infoheader.bitsPerPixel == 24) && !image->infoheader.compression)) {
		printf("Error bmp file is not 24 bits image\n");
        return EBMP;
	}
    image->pixel=(PIXEL *)malloc(sizeof(PIXEL)*image->infoheader.cols*image->infoheader.rows);
    totpixs=image->infoheader.rows*image->infoheader.cols;
    while(i<totpixs)
    {
        fread(image->pixel+i, sizeof(PIXEL), 512, fin);
        i += 512;
    }

    fclose(fin);
}

/* libera memoria para la imagen */
int closeBMP(IMAGE * image) {
	if (image) {
		if (image->pixel) {
			free (image->pixel);
			return 0;
		}
	}
	return -1;
}


int saveBMP(char *filename, IMAGE *image)
{
    FILE *fout;
    int i,totpixs;
    fout=fopen(filename,"wb");
    if (fout == NULL)
        return EOPEN; // Error
	// Escribe encabezado
    fwrite(&image->header, sizeof(HEADER), 1, fout);
	// Escribe información del encabezado
    fwrite(&image->infoheader, sizeof(INFOHEADER), 1, fout);
    i=0;
    totpixs=image->infoheader.rows*image->infoheader.cols;

    while(i<totpixs)
    {
        fwrite(image->pixel+i, sizeof(PIXEL), 512, fout);
        i += 512;
    }
    fclose(fout);
}


unsigned char blackandwhite(PIXEL p)
{
    return((unsigned char) (0.3*((float)p.red)+ 0.59*((float)p.green)+0.11*((float)p.blue)));
}


void * thread_work(void * arg) {
    int i,j;
	int count = 0;
	int imageCols;
	IMAGE * imagefte, *imagedst;
    PIXEL *pfte,*pdst;
    PIXEL *v0,*v1,*v2,*v3,*v4,*v5,*v6,*v7;
	struct thread_param * param;
	param = (struct thread_param *) arg;
	imageCols = param->imagefte->infoheader.cols;
	imagefte = param->imagefte;
	imagedst = param->imagedst;

	for(i = param->start; i <= param->end; i++)

		for(j = 0; j < imageCols; j++)
		{
			pfte = imagefte->pixel + imageCols * i + j;
			v0 = pfte - imageCols - 1;
			v1 = pfte - imageCols;
			v2 = pfte - imageCols + 1;
			v3=pfte-1;
			v4=pfte+1;
			v5=pfte+imageCols-1;
			v6=pfte+imageCols;
			v7=pfte+imageCols+1;
			pdst=imagedst->pixel+imageCols*i+j;
			if(abs(blackandwhite(*pfte)-blackandwhite(*v0))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v1))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v2))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v3))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v4))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v5))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v6))>DIF ||
					abs(blackandwhite(*pfte)-blackandwhite(*v7))>DIF)
			{
				pdst->red=0;
				pdst->green=0;
				pdst->blue=0;
			}
			else
			{
				pdst->red=255;
				pdst->green=255;
				pdst->blue=255;
			}
		}

}

void processBMP(IMAGE *imagefte, IMAGE *imagedst)
{
	int i;
    int imageRows,imageCols;
	pthread_t tid[n_threads];
	struct thread_param param[n_threads];
	unsigned int size_data;

    memcpy(imagedst,imagefte,sizeof(IMAGE)-sizeof(PIXEL *));
    imageRows = imagefte->infoheader.rows;
    imageCols = imagefte->infoheader.cols;
    imagedst->pixel=(PIXEL *)malloc(sizeof(PIXEL)*imageRows*imageCols);

	size_data = imageRows / n_threads;

	for (i = 0; i < n_threads; i++) {
		param[i].start = i * size_data;
		param[i].end = (i + 1) * size_data - 1;
		param[i].imagefte = imagefte;
		param[i].imagedst = imagedst;
		pthread_create(&tid[i], NULL, thread_work, &param[i]);
	}

	for (i = 0; i < n_threads; i++) {
		pthread_join(tid[i], NULL);
	}

}


void help(char * progname) {
	fprintf(stdout, "Usage: %s [options]\n", progname);
	fputs(
			"\t-h | -a | --help | --ayuda\t\t Esta informacion\n"
			"\t-v | --version\t\tMuestra la version del programa\n"
			"\n"
			"\tBanderas de ejecucion:\n"
			"\t[-o | --origen] <ruta>\t\tRuta de la imagen original\n"
			"\t[-d | --destino] <ruta>\t\tRuta de la imagen destino\n"
			"\t[-t | --threads] n]\t\tNumero de hilos a utilizar\n\n", stdout);

}


int main(int argc, char *argv[])
{
    int res;
    long long start_ts;
    long long stop_ts;
    long long elapsed_time;
    long lElapsedTime;
    struct timeval ts;
    char nameorig[100];
    char namedest[100];
	int flags_en = 0x0;
	int choice;

	while (1)
	{
		static struct option long_options[] =
		{
			/* Use flags like so:
			{"verbose",	no_argument,	&verbose_flag, 'V'}*/
			/* Argument styles: no_argument, required_argument, optional_argument */
			{"version", no_argument,	0,	'v'},
			{"ayuda",	no_argument,	0,	'a'},
			{"help",	no_argument,	0,	'h'},
			{"origen",	required_argument,	0,	'o'},
			{"destino",	required_argument,	0,	'd'},
			{"threads",	required_argument,	0,	't'},
			{0,0,0,0}
		};

		int option_index = 0;

		/* Argument parameters:
			no_argument: " "
			required_argument: ":"
			optional_argument: "::" */

		choice = getopt_long( argc, argv, "vaho:d:t:",
					long_options, &option_index);

		if (choice == -1)
			break;

		switch( choice )
		{
			case 'v':
				fprintf(stdout, "Version: 0.0\n");
				return 0;

			case 'h': case 'a':
				help(argv[0]);
				return 0;
				break;

			case 'o':
				flags_en = flags_en | 0x1;
				sprintf(nameorig, "%s", optarg);
				break;

			case 'd':
				flags_en = flags_en | 0x2;
				sprintf(namedest, "%s", optarg);
				break;

			case 't':
				flags_en = flags_en | 0x4;
				n_threads = atoi(optarg);
				if (n_threads <= 0)
					n_threads = 1;
				break;

			case '?':
				/* getopt_long will have already printed an error */
				break;

			default:
				return EXIT_FAILURE;
		}
	}

	/* Deal with non-option arguments here */
	if ( optind < argc )
	{
		while ( optind < argc )
		{}
	}



	if (!(flags_en & 0x1)) {
		printf("Ruta de la imagen origen: ");
		scanf("%s", nameorig);
	}

	if (!(flags_en & 0x2)) {
		printf("Ruta de la imagen destino: ");
		scanf("%s", namedest);
	}

	if (!(flags_en & 0x4)) {
		printf("Numero de threads: ");
		scanf("%d", &n_threads);
		if (n_threads <= 0)
			n_threads = 1;
	}


    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec * 1000000 + ts.tv_usec; // Tiempo inicial
    printf("Archivo fuente %s\n", nameorig);
    printf("Archivo destino %s\n", namedest);
    printf("Threads utilizados %d\n", n_threads);

	imagenfte.pixel = NULL;
	imagendst.pixel = NULL;
    res = loadBMP(nameorig, &imagenfte);
    if(res < 0)
    {
        fprintf(stderr,"No se puede procesar la imagen \"%s\"\n", nameorig);
        exit(1);
    }
    printf("Procesando imagen de: Renglones = %d, Columnas = %d\n",imagenfte.infoheader.rows,imagenfte.infoheader.cols);
    processBMP(&imagenfte,&imagendst);
    res=saveBMP(namedest,&imagendst);
    if(res == -1)
    {
        fprintf(stderr,"Error al escribir imagen\n");
        exit(1);
    }
	closeBMP(&imagenfte);
	closeBMP(&imagendst);
    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec * 1000000 + ts.tv_usec; // Tiempo final
    elapsed_time = stop_ts - start_ts;
    printf("Tiempo = %4.2f segundos\n",(float)elapsed_time/1000000);
}
