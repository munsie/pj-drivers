// Command definition
// Message format
// Bits   0123 4567 8901 2345 6789
// Binary LCCC DDDD DDDD DDDD DDDD
// CCC is device definition
// if L = 0 DDDD is 8 bytes
// if L = 1 DDDD is 16 bytes

// Value size
#define J4B_MSG_VAL_SIZE      0b1000
#define J4B_MSG_CMD           0b0111
// Value size
#define J4B_MSG_8BIT_VAL      0b0000
#define J4B_MSG_16BIT_VAL     0b1000

// Device definitions
#define J4B_DEV_INPUT_0       0b0000
#define J4B_DEV_INPUT_1       0b0001
#define J4B_DEV_SOUND         0b0010
#define J4B_DEV_OUTPUT        0b0011
#define J4B_DEV_METRICS_5V    0b0100
#define J4B_DEV_METRICS_12V   0b0101
#define J4B_DEV_METRICS_5VPI  0b0110
#define J4B_DEV_METRICS_33VPI 0b0111

// Command definitions
#define J4B_CMD_INPUT_0       (J4B_MSG_16BIT_VAL | J4B_DEV_INPUT_0)
#define J4B_CMD_INPUT_1       (J4B_MSG_16BIT_VAL | J4B_DEV_INPUT_1)
#define J4B_SOUND             (J4B_MSG_8BIT_VAL | J4B_DEV_SOUND)
#define J4B_OUTPUT            (J4B_MSG_8BIT_VAL | J4B_DEV_OUTPUT)
#define J4B_METRICS_5V        (J4B_MSG_8BIT_VAL | J4B_DEV_METRICS_5V)
#define J4B_METRICS_12V       (J4B_MSG_8BIT_VAL | J4B_DEV_METRICS_12V)
#define J4B_METRICS_5VPI      (J4B_MSG_8BIT_VAL | J4B_DEV_METRICS_5VPI)
#define J4B_METRICS_33VPI     (J4B_MSG_8BIT_VAL | J4B_DEV_METRICS_33VPI)
#define J4B_MAX_DEVICES 8

struct j4b_msg {
	unsigned char cmd:4;
	unsigned short val:16;
};

int j4b_getValue(int mdev, int *val);
int j4b_putValue(int mdev, int val);
