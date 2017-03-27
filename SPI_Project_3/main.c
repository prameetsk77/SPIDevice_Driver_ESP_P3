#include <stdint.h>
//#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <signal.h>

void update_display(char disp_array[] , int fd_SPI , struct spi_ioc_transfer xfer , char* writeBuf){
	int i = 0;	
	for(i = 1 ; i<= 8 ; i++){
	
	writeBuf[1] = disp_array[i-1]; // Step 1
	writeBuf[0] = (char)i;  //address
	
	if(ioctl(fd_SPI, SPI_IOC_MESSAGE(1), &xfer) < 0)
		perror("SPI Message Send Failed");
	}
}

int main(int argc, char* argv[])
{
	int fd_SPI = 0 , fd_gpio= 0, fd_UDM =0,ret=0;
	unsigned char mode = 0;
	unsigned char lsb = 0; 
	unsigned char bpw = 8;
	unsigned long ms = 10000000;
	int echo_dist=0;
	char readBuf[2];
	char writeBuf[2];
	struct spi_ioc_transfer xfer;
	void siginthandler(int);
	int safe_distance = 0;
	int toggle = 0;
	char disp_array[][8] = { 
		{0x81 , 0x42 , 0x24 , 0x18 , 0x18 , 0x24 , 0x42 , 0x81 } ,  // X
		{0x0,0x0,0x0,0xFF,0xFF,0x0,0x0,0x0} ,  // 	 		|| 
		{0x0,0x0,0xFF,0xFF,0xFF,0xFF,0x0,0x0} ,  //	       ||||
		{0x0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x0} ,  //     ||||||
		{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF} , //   ||||||||
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}  //   	   BLANK
	};
	
	signal(SIGINT, siginthandler);
	
	if(argc ==2){
		safe_distance = atoi(argv[1]);
		printf("Safe Distance set to %d \n" , safe_distance);
	}
	else {
		safe_distance = 7;
		printf("Setting to default safe distance. Safe Distance = %d",  safe_distance);
	}
	
	
	fd_gpio = open("/sys/class/gpio/export", O_WRONLY);
	write(fd_gpio, "42", 2);
	close(fd_gpio); 
	
	// Set GPIO Direction
	fd_gpio = open("/sys/class/gpio/gpio42/direction", O_WRONLY);
	write(fd_gpio, "out", 3);
	close(fd_gpio);
	
	// Set Value
	fd_gpio = open("/sys/class/gpio/gpio42/value", O_WRONLY);
	write(fd_gpio, "0", 1);



	fd_gpio = open("/sys/class/gpio/export", O_WRONLY);
	write(fd_gpio, "43", 2);
	close(fd_gpio); 
	
	// Set GPIO Direction
	fd_gpio = open("/sys/class/gpio/gpio43/direction", O_WRONLY);
	write(fd_gpio, "out", 3);
	close(fd_gpio);
	
	// Set Value
	fd_gpio = open("/sys/class/gpio/gpio43/value", O_WRONLY);
	write(fd_gpio, "0", 1);
    close(fd_gpio);
	
	fd_gpio = open("/sys/class/gpio/export", O_WRONLY);
	write(fd_gpio, "55", 2);
	close(fd_gpio); 
	
	// Set GPIO Direction
	fd_gpio = open("/sys/class/gpio/gpio55/direction", O_WRONLY);
	write(fd_gpio, "out", 3);
	close(fd_gpio);
	
	// Set Value
	fd_gpio = open("/sys/class/gpio/gpio55/value", O_WRONLY);
	write(fd_gpio, "0", 1);
	close(fd_gpio);

	fd_SPI = open("/dev/spidev1.0", O_RDWR);

	mode |= SPI_MODE_0;
	
	// Mode Settings could include:
	// 		SPI_LOOP		0
	//		SPI_MODE_0		1
	//		SPI_MODE_1		2
	//		SPI_MODE_2		3
	//		SPI_LSB_FIRST	4
	//		SPI_CS_HIGH		5
	//		SPI_3WIRE		6
	//		SPI_NO_CS		7		

	// Setup Commands could include:
	//		SPI_IOC_RD_MODE
	//		SPI_IOC_WR_MODE
	//		SPI_IOC_RD_LSB_FIRST
	//		SPI_IOC_WR_LSB_FIRST
	//		SPI_IOC_RD_BITS_PER_WORD
	//		SPI_IOC_WR_BITS_PER_WORD
	//		SPI_IOC_RD_MAX_SPEED_HZ
	//		SPI_IOC_WR_MAX_SPEED_HZ
	
	if(ioctl(fd_SPI, SPI_IOC_WR_MODE, &mode) < 0)
	{
		perror("SPI Set Mode Failed");
		close(fd_SPI);
		return -1;
	}
	
	if(ioctl(fd_SPI, SPI_IOC_WR_LSB_FIRST, &lsb) < 0)
	{
		perror("SPI Set LSB Failed");
		close(fd_SPI);
		return -1;
	}

if(ioctl(fd_SPI, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0)
	{
		perror("SPI Set LSB Failed");
		close(fd_SPI);
		return -1;
	}

if(ioctl(fd_SPI, SPI_IOC_WR_MAX_SPEED_HZ, &ms) < 0)
	{
		perror("SPI Set LSB Failed");
		close(fd_SPI);
		return -1;
	}
	
// Steps to setup transaction
//1. 0x00 to reg 0x09; 2. 0x03 to 0x0A ; 3. 0x07 to reg 0x0B ; 4. 0x01 to reg 0x0C; 5.(test) 0x01 to reg 0x0F OR (no test) 0x00 to reg 0x0F
	// Setup a Write Transfer
	
	// Setup Transaction 
	xfer.tx_buf = (unsigned long)writeBuf;
	xfer.rx_buf = (unsigned long)NULL;
	xfer.len = 2; // Bytes to send
	xfer.cs_change = 0;
	xfer.delay_usecs = 0;
	xfer.speed_hz = 10000000;
	xfer.bits_per_word = 8;


	writeBuf[1] = 0x00; // Step 1 - 
	writeBuf[0] = 0x09; // Decode mode register address of IC 7219
	
	// Send Message
	if(ioctl(fd_SPI, SPI_IOC_MESSAGE(1), &xfer) < 0)
		perror("SPI Message Send Failed");

	writeBuf[1] = 0x03; // Step 2
	writeBuf[0] = 0x0A; // Intensity register address of IC 7219
	
	// Send Message
	if(ioctl(fd_SPI, SPI_IOC_MESSAGE(1), &xfer) < 0)
		perror("SPI Message Send Failed");

	writeBuf[1] = 0x07; // Step 3
	writeBuf[0] = 0x0B; // Scan Limit register address of IC 7219
	
	// Send Message
	if(ioctl(fd_SPI, SPI_IOC_MESSAGE(1), &xfer) < 0)
		perror("SPI Message Send Failed");

	writeBuf[1] = 0x01; // Step 4
	writeBuf[0] = 0x0C; // Shutdown register address of IC 7219
	
	// Send Message
	if(ioctl(fd_SPI, SPI_IOC_MESSAGE(1), &xfer) < 0)
		perror("SPI Message Send Failed");
	
	
	fd_UDM =  open("/dev/Ultrasonic_HCSR04", O_RDONLY);    
	
	if(fd_UDM < 0)
	{
		printf("Error: could not open UDM device.\n");
		return -1;
	}
	
	while(1)
	{
		ret = read(fd_UDM,&echo_dist, sizeof(int));
		if(ret < 0)
		{
			 perror("Error: ");
			return -1;
		}
		
		if(echo_dist<=safe_distance){
			/* Flash X by toggling between 'X' and blank */
			if(!toggle){
				/*display 'X'*/
				update_display(disp_array[0], fd_SPI , xfer,  writeBuf);
				toggle = !toggle;
			}
			else if(toggle){
				/*display blank*/
				update_display(disp_array[5], fd_SPI , xfer,  writeBuf);
				toggle = !toggle;			
			}
		}
		else if((echo_dist>(safe_distance)) && (echo_dist<=(safe_distance+10))){	
			update_display(disp_array[1], fd_SPI , xfer,  writeBuf);
			toggle = 0;
		}
		else if((echo_dist>(safe_distance+10)) && (echo_dist<=(safe_distance+30))){	
			update_display(disp_array[2], fd_SPI , xfer,  writeBuf);
			toggle = 0;
		}
		else if((echo_dist>(safe_distance+30)) && (echo_dist<=(safe_distance+40))){
			update_display(disp_array[3], fd_SPI , xfer,  writeBuf);
			toggle = 0;
		}
		else{
			update_display(disp_array[4], fd_SPI , xfer,  writeBuf);
			toggle = 0;
		}
		usleep(120 * 1000);
	}
	close(fd_SPI);
}
 
void siginthandler(int signum)
{
   printf("Caught signal %d, ending program...\n", signum);

   exit(1);//DOES A CLEAN EXIT(closes all file descriptors)
}

