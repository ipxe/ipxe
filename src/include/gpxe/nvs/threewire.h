#ifndef _GPXE_NVS_THREEWIRE_H
#define _GPXE_NVS_THREEWIRE_H

/** @file
 *
 * Three-wire serial interface
 *
 */

struct threewire;

/** Three-wire interface methods */
struct threewire_operations {
	/**
	 * Set status of Chip Select line
	 *
	 * @v three	Three-wire interface
	 * @v cs	New status for chip select line
	 */
	void ( * setcs ) ( struct threewire *three, int cs );
	/**
	 * Set status of Serial Clock line
	 *
	 * @v three	Three-wire interface
	 * @v sk	New status for serial clock line
	 */
	void ( * setsk ) ( struct threewire *three, int sk );
	/**
	 * Set status of Data Input line
	 *
	 * @v three	Three-wire interface
	 * @v di	New status for data input line
	 */
	void ( * setdi ) ( struct threewire *three, int di );
	/**
	 * Get status of Data Output line
	 *
	 * @v three	Three-wire interface
	 * @ret do	Status of data output line
	 */
	int ( * getdo ) ( struct threewire *three );
};

/**
 * A three-wire serial interface
 *
 * This interface consists of a clock line (SK), data input (DI) and
 * data output (DO).  There is also a chip select line (CS) which is
 * integral to the operation of the device, but Atmel still calls it a
 * three-wire interface.
 *
 */
struct threewire {
	/** Interface methods */
	struct threewire_operations *ops;
	/** Address size (in bits) */
	unsigned int adrsize;
	/** Data size (in bits) */
	unsigned int datasize;
	/** Delay between SK transitions (in us) */
	unsigned int udelay;
};

/**
 * Calculate read command for a specified address
 *
 * @v three	Three-wire interface
 * @v address	Address
 * @ret cmd	Command
 */
static inline __attribute__ (( always_inline )) unsigned long
threewire_cmd_read ( struct threewire *three, unsigned long address ) {
	return ( ( 0x6 << three->adrsize ) | address );
}

/**
 * Calculate command length
 *
 * @v three	Three-wire interface
 * @ret len	Command length, in bits
 */
static inline __attribute__ (( always_inline )) int
threewire_cmd_len ( struct threewire *three ) {
	return ( three->adrsize + 3 );
}

/* Constants for some standard parts */
#define AT93C46_ORG8_ADRSIZE	7
#define AT93C46_ORG8_DATASIZE	8
#define AT93C46_ORG16_ADRSIZE	6
#define AT93C46_ORG16_DATASIZE	16
#define AT93C46_UDELAY		1
#define AT93C56_ORG8_ADRSIZE	9
#define AT93C56_ORG8_DATASIZE	8
#define AT93C56_ORG16_ADRSIZE	8
#define AT93C56_ORG16_DATASIZE	16
#define AT93C56_UDELAY		1

extern unsigned long threewire_read ( struct threewire *three,
				      unsigned long address );

#endif /* _GPXE_NVS_THREEWIRE_H */
