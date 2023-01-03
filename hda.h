/* HDA Header File */
//References:
// 1:  https://git.tyndur.org/lowlevel/tyndur/-/tree/master/src/modules/cdi/hdaudio
// 2: https://www.intel.com/content/www/us/en/standards/high-definition-audio-specification.html
#ifndef _HDA_H
#define _HDA_H
#include "../types.h"
#include "PCI.h"
struct hda_bdl_entry {
    uint32_t paddr;
    uint32_t length;
    uint32_t flags;
};

struct hda_output {



    uint8_t     codec;
    uint16_t    node_id;
 
    uint32_t    sample_rate;
    int         amp_gain_steps;
    int         num_channels;
} hda_output;


typedef struct hda_device {

    struct hda_output output;
    uint32_t mmio_size;
    

    uint32_t *mmio_base;

    uint32_t * buffer;
    volatile uint32_t*      corb;           ///< Command Outbound Ring Buffer
    volatile uint32_t*      rirb;           ///< Response Inbound Ring Buffer
    volatile struct hda_bdl_entry*   bdl;            ///< Buffer Descriptor List
    volatile uint32_t*      dma_pos;        ///< DMA Position in Current Buffer

    uint32_t                corb_entries;   ///< Number of CORB entries
    uint32_t                rirb_entries;   ///< Number of RIRB entries
    uint16_t                rirb_read_pointer;         ///< RIRB Read Pointer

    int                     buffers_completed;
} hda_device;


void hda_interrupt_handler();
//*************************************************
/*
 * CORB and RIRB functions
 */


//Initializes the CORB and RIRB. must be called on reset.
void HDA_rirb_init();
void HDA_corb_init();
//Writes to CORB/RIRB. RIRB should never be written to.
void HDA_corb_write(uint32_t verb);
void HDA_rirb_write();
//Reads from CORB/RIRB. CORB should never be read from.
void HDA_corb_read();
void HDA_rirb_read();

/*
END OF CORB/RIRB FUNCTIONS
*/
//*************************************************
//*************************************************
/*
START OF CODEC FUNCTIONS
*/

uint32_t HDA_codec_query(uint8_t codec, uint32_t nid, uint32_t payload);

void HDA_init_out_widget();
void HDA_config_out_widget();

void HDA_widget_init(uint8_t codec, uint16_t node_id);
void HDA_widget_dump_connections();

int HDA_codec_list_widgets(uint8_t codec);

void HDA_list_codecs();
/*
END OF CODEC FUNCTIONS
*/
//*************************************************
//*************************************************
/*
START OF INTERFACE FUNCTIONS
*/


void HDA_reset();

void HDA_init_stream_descriptor();
void HDA_init_dev();

void HDA_set_volume(uint8_t vol);
void HDA_set_sampling_rate();
void HDA_set_channels();

void HDA_send_data();

void HDA_get_status();
void HDA_set_status();

void HDA_get_position();
void HDA_set_position();

/*
END OF INTERFACE FUNCTIONS
*/
//*************************************************

#endif

