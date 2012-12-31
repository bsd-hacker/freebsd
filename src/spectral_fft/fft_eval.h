#ifndef	__FFT_EVAL_H__
#define	__FFT_EVAL_H__

typedef int8_t s8;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

/* taken from ath9k.h */
#define SPECTRAL_HT20_NUM_BINS          56

enum ath_fft_sample_type {
        ATH_FFT_SAMPLE_HT20 = 0
};

struct fft_sample_tlv {
        u8 type;        /* see ath_fft_sample */
        u16 length;
        /* type dependent data follows */
} __attribute__((packed));

struct fft_sample_ht20 {
        struct fft_sample_tlv tlv;

        u8 __alignment;

        u16 freq;
        s8 rssi;
        s8 noise;

        u16 max_magnitude;
        u8 max_index;
        u8 bitmap_weight;

        u64 tsf;

        u16 data[SPECTRAL_HT20_NUM_BINS];
} __attribute__((packed));


struct scanresult {
        struct fft_sample_ht20 sample;
        struct scanresult *next;
};

#endif	/* __FFT_EVAL_H__ */
