/*
keywords for google (so that future people may not suffer the same mistakes I did): INTEL HDA DRIVER, INTEL HIGH DEFINITION AUDIO DRIVER, INTEL HD AUDIO DRIVER, HD AUDIO DRIVER, ECE391 INTEL HDA
if you're at uiuc & in ECE391, message me if you want any help with this!
One notable detail for ECE391 is that you will need to modify your QEMU boot line to include the following line:
-device intel-hda -device hda-duplex

The following document file is a slice of an audio driver written as part of the ECE391 final competition.
It is barebones, but follows the original 2004 intel document as closesly as possible (within the constraints of QEMU).
Requirements:
 Low level of PCI interfacing 
 Memory allocation function.  
References:
 https://www.intel.com/content/www/us/en/standards/high-definition-audio-specification.html
 https://git.tyndur.org/lowlevel/tyndur/-/blob/master/src/modules/cdi/hdaudio/main.c
 https://wiki.osdev.org/Intel_High_Definition_Audio
 https://forum.osdev.org/viewtopic.php?f=1&t=34525
Special thanks to:
David Cooper, for his work on the OSDev HDA wiki page, and helpful forum posts.
*/


pci_dev_t pci_hda_dev;
uint32_t rings[4096];
uint8_t kb[8];
#define HDA_MEMORY_LOCATION 0xFEBF0000
#define BDL_SIZE                      4
#define BUFFER_SIZE             0x10000
#define HDA_MEMORY_END 0x80000 + 0x4000
#define CORBSIZE 0x4E
#define RIRBSIZE 0x5E
#define CORBLBASE 0x40
#define RIRBLBASE 0x50
#define RIRBSTS 0x5d
#define RIRBCTL 0x5C
#define CORBCTL 0x4C
#define INTSTS 0x24
#define INTCTL 0x20
#define BIT_2 0x1 << 1
#define RIRBCTL_RIRBRUN BIT_2
#define CORBCTL_CORBRUN BIT_2
#define CORBWP 0x48
#define GCTL 0x08
#define BIT_1 0x1 << 0
#define CRST BIT_1
#define WAKEEN 0x0C
#define STATESTS 0x0E
#define SDIN_LEN 16
#define CORBRP  0x4a
#define RIRBWP 0x58
#define BUFFER_SIZE 0x10000


/* VERBS! */
//Shifted over byte so it's easier to OR with payload
//skating by with these...
#define VERB_GET_PARAMETER      0xf0000
#define VERB_SET_STREAM_CHANNEL 0x70600
#define VERB_SET_FORMAT         0x20000
#define VERB_GET_AMP_GAIN_MUTE  0xb0000
#define VERB_SET_AMP_GAIN_MUTE  0x30000
#define VERB_GET_CONFIG_DEFAULT 0xf1c00
#define VERB_GET_CONN_LIST      0xf0200
#define VERB_GET_CONN_SELECT    0xf0100
#define VERB_GET_PIN_CONTROL    0xf0700
#define VERB_SET_PIN_CONTROL    0x70700
#define VERB_GET_EAPD_BTL       0xf0c00
#define VERB_SET_EAPD_BTL       0x70c00
#define VERB_GET_POWER_STATE    0xf0500
#define VERB_SET_POWER_STATE    0x70500
/* PARAMS! */
#define PARAM_SUB_NODE_COUNT      0x04
#define PARAM_FUNCTION_GROUP_TYPE       0x05
#define PARAM_WIDGET_CAPABILITIES 0x09
#define PARAM_OUT_AMP_CAP         0x12
#define PARAM_PIN_CAPABILITIES    0x0c
 
/* WIDGETS! */

#define WIDGET_OUTPUT           0x0
#define WIDGET_INPUT            0x1
#define WIDGET_MIXER            0x2
#define WIDGET_SELECTOR         0x3
#define WIDGET_PIN_COMPLEX      0x4
#define WIDGET_POWER            0x5
#define WIDGET_VOLUME_KNOB      0x6
#define WIDGET_BEEP_GEN         0x7
#define WIDGET_VENDOR_DEFINED   0xF
#define PARAM_CONN_LIST_LEN     0xE




void wait_input(){
    terminal_read(0, kb, 1);   
}
struct hda_device audio_device;

static void hda_reg_writebyte(uint32_t reg_offset, uint8_t val){
    (*((volatile uint8_t*)(reg_offset + HDA_MEMORY_LOCATION)))=(val);
}
static void hda_reg_writeword(uint32_t reg_offset, uint16_t val){
    (*((volatile uint16_t*)(reg_offset + HDA_MEMORY_LOCATION)))=(val);
}
static void hda_reg_writelong(uint32_t reg_offset, uint32_t val){
    (*((volatile uint32_t*)(reg_offset + HDA_MEMORY_LOCATION)))=(val);
}


static uint8_t hda_reg_readbyte(uint32_t reg_offset){
    return *((volatile uint8_t*)(reg_offset + HDA_MEMORY_LOCATION));
}

static uint16_t hda_reg_readword(uint32_t reg_offset){
    return *((volatile uint16_t*)(reg_offset + HDA_MEMORY_LOCATION));
}

static uint32_t hda_reg_readlong(uint32_t reg_offset){
    return *((volatile uint32_t*)(reg_offset + HDA_MEMORY_LOCATION));
}

void hda_interrupt_handler(){
    //uint32_t int_status = hda_reg_readlong(INTSTS);
    //TODO: THIS!
}

void HDA_rirb_init(){
    uint8_t reg;
    uint32_t base;
    uint32_t intermediate;
    reg = hda_reg_readbyte(RIRBSIZE);
    if((reg & 0x20) != 0){printf("RIRBSIZE = 128B\n");
        reg |= 0x1;
        audio_device.rirb_entries = 16;
        }
    else if((reg & 0x40) != 0){printf("RIRBSIZE = 2048B\n");
        reg |= 0x2;
        audio_device.rirb_entries = 256;
        }
    else if((reg & 0x10) != 0){printf("RIRBSIZE = 16B\n");
    audio_device.rirb_entries = 2;
    }
    //SET DMA WRAP-AROUND POINT! with these bits! page 44
    hda_reg_writebyte(RIRBSIZE, reg);
    printf("RIRBSIZE: 0x%x\n", hda_reg_readbyte(RIRBSIZE));
    intermediate = (uint32_t)malloc(audio_device.rirb_entries*8*2);
    intermediate += 0xFF;
    intermediate &= ~0x7F;
    printf("Intermediate 0x%x\n", intermediate);
    audio_device.rirb = (uint32_t*) intermediate;
    base = (uint32_t)audio_device.rirb;

    hda_reg_writelong(RIRBLBASE, base);

    printf("RIRBBase address int: 0x%x\n", base);
    printf("RIRBBase address from read: 0x%x%x\n", hda_reg_readlong(RIRBLBASE + 4), hda_reg_readlong(RIRBLBASE));
    hda_reg_writeword(0x5A, 0x42);
    //RUN THE DMA ENGINE!!! yeehaw
    
    hda_reg_writebyte(RIRBCTL, BIT_2);
    printf("DMA_ENGINE STATUS: 0x%x\n", hda_reg_readbyte(RIRBCTL));

}

void HDA_corb_init(){
    uint8_t reg;
    uint32_t base;
    uint32_t intermediate;
    hda_reg_writeword(0x5A, 0xFF);
    reg = hda_reg_readbyte(CORBSIZE);
    if((reg & 0x20) != 0){printf("CORBSIZE = 128B\n");
        reg |= 0x1;
        audio_device.corb_entries = 16;
        }
    else if((reg & 0x40) != 0){printf("CORBSIZE = 2048B\n");
        reg |= 0x2;
        audio_device.corb_entries = 256;
        }
    else if((reg & 0x10) != 0){printf("CORBSIZE = 16B\n");
    audio_device.corb_entries = 2;
    }
    //SET DMA WRAP-AROUND POINT! with these bits! page 44
    hda_reg_writebyte(CORBSIZE, reg);
    intermediate = (uint32_t)malloc(audio_device.corb_entries*8*2);
    //Bodge so I don't have to worry about the bottom 6 bits being 0.
    intermediate += 0xFF;
    intermediate &= ~0x7F;
    
    audio_device.corb = (uint32_t*) intermediate;
    base = (uint32_t)audio_device.corb;
    hda_reg_writelong(CORBLBASE, base);

    printf("CORBBase address int: 0x%x\n", base);
    printf("CORBBase address from read: 0x%x%x\n", hda_reg_readlong(CORBLBASE + 4), hda_reg_readlong(CORBLBASE));
    
    //RUN THE DMA ENGINE!!! yeehaw
    hda_reg_writebyte(CORBCTL, BIT_2);
}
    
void HDA_corb_write(uint32_t verb){
    uint16_t write_pointer;
    uint16_t read_pointer;
    uint16_t next;
    
    write_pointer = hda_reg_readbyte(CORBWP);
    //printf("CORB_W_PTR:0x%x \n", write_pointer);
    next = (write_pointer + 1) % audio_device.corb_entries;
    
    do{
        read_pointer = hda_reg_readword(CORBRP) & 0xFF;
    }while((next == read_pointer));
    audio_device.corb[next] = verb;
    
    hda_reg_writeword(CORBWP, next);
    // printf("CORB_ADDR_SAVED 0x%x\n", (uint16_t *)audio_device.corb + next);
    // printf("CORB_ADDR_WRITTEN 0x%x\n", hda_reg_readlong(CORBLBASE) + next);
    // printf("CORB_ADDR_  0x%x\n", hda_reg_readlong(CORBLBASE));
    // printf("CORB_ADDR_  0x%x\n", audio_device.corb);
}

//for symmetry
void HDA_rirb_write(){return;}

//for symmetry
void HDA_corb_read(){return;}

void HDA_rirb_read(uint32_t* response){
    uint16_t write_pointer;
    uint16_t read_pointer;
    uint8_t i;

    read_pointer = audio_device.rirb_read_pointer;
    do{
        write_pointer = hda_reg_readword(RIRBWP) & 0xFF;
    }while (write_pointer == read_pointer);
    
    read_pointer = (read_pointer + 1) % audio_device.rirb_entries;
    audio_device.rirb_read_pointer = read_pointer;
    //The RIRB should have 64 bit responses, but we only ever care about the bottom 32 bits...
    *response = audio_device.rirb[read_pointer*2];
    printf("RIRB response: 0x%x\n", *response);
    for( i = 1; i < 10; i++){
        if(audio_device.rirb[read_pointer*2 + i*2] != 0){
            printf("LOST RIRB SYNCHRONICITY!!!\n");
            printf("RIRB contents at %d: 0x%x\n", i, audio_device.rirb[read_pointer*2 + i*2]);
            
        }
    }

    hda_reg_writebyte(RIRBSTS, 5);
}

uint32_t HDA_codec_query(uint8_t codec, uint32_t nid, uint32_t payload){
    uint32_t response;
    uint32_t verb = ((codec & 0xF) << 28) | 
                    ((nid & 0xFF) << 20) |
                    ((payload & 0xFFFFF));
    printf("\nCodec Query: 0x%x\n", verb);
    HDA_corb_write(verb);
    printf("Finished write\n");
    HDA_rirb_read(&response);
    printf("finished read: 0x%x\n", response);
    return response;
}
// 
 void HDA_init_out_widget(){
}
// void HDA_config_out_widget(){}
// 
void HDA_widget_init(uint8_t codec, uint16_t node_id){
    uint32_t widget_capabilities;
    uint16_t widget_type;
    uint32_t amp_capabilities;
    uint32_t eapd_btl;
    widget_capabilities = HDA_codec_query(codec, node_id, VERB_GET_PARAMETER | PARAM_WIDGET_CAPABILITIES);
    //incapable widget. // new insult.
    if(widget_capabilities == 0){
        printf("FOUND INCAPABLE WIDGET\n");
        return;
    }
    //0xF00000 widget mask, 20 to get that widget in first few bits.
    widget_type = (widget_capabilities & 0xF00000) >> 20;
    //printf("WIDGET FOUND!!!: 0x%x \n", widget_type);
    amp_capabilities = HDA_codec_query(codec, node_id, VERB_GET_PARAMETER | PARAM_OUT_AMP_CAP);
    eapd_btl = HDA_codec_query(codec, node_id, VERB_GET_EAPD_BTL);
    //format is now bit 7 = mute bit, 6:0 = gain
    printf("WIDGET_FOUND! 0x%x\n", widget_type);

    switch(widget_type){
        case WIDGET_OUTPUT          :{
            if(!audio_device.output.node_id){
                printf("OUTPUT FOUND\n");
                audio_device.output.codec = codec;
                audio_device.output.node_id = node_id;
                audio_device.output.amp_gain_steps = (amp_capabilities >> 8) & 0x7F;
                
            }
            HDA_codec_query(codec, node_id, VERB_SET_EAPD_BTL | eapd_btl | 0x2);
        break;}
        case WIDGET_INPUT           :
        //Not supported

        break;
        case WIDGET_MIXER           :
        //Not supported

        break;
        case WIDGET_SELECTOR        :
        //Not supported

        break;
        case WIDGET_PIN_COMPLEX     :{
            uint32_t pin_capabilities;
            printf("PIN FOUND\n");

            pin_capabilities = HDA_codec_query(codec, node_id, VERB_GET_PARAMETER | PARAM_PIN_CAPABILITIES);
            
            //If it's the output pin, we care. Otherwise. toss it.
            if((pin_capabilities & (1 << 4)) == 0){
                return;
            }
            printf("OUTPUT PIN FOUND\n");

            uint32_t pin_cntl = HDA_codec_query(codec, node_id, VERB_GET_PIN_CONTROL);
            //6th bit enables the output amp stream
            pin_cntl |= (1 << 6);
            HDA_codec_query(codec, node_id, VERB_SET_PIN_CONTROL | pin_cntl);
            HDA_codec_query(codec, node_id, VERB_SET_EAPD_BTL | eapd_btl | 0x2);
        break;}
        case WIDGET_POWER           :
        //Not supported

        break;
        case WIDGET_VOLUME_KNOB     :
        //Not supported

        break;          
        case WIDGET_BEEP_GEN        :
        //Not supported
        

        break;
        case WIDGET_VENDOR_DEFINED  :
        //Not supported


        break;
        default:
        //reserved
        break;
    }
    if(widget_capabilities & (0x1 << 10)){
        //If the widget has power control, turn it on!!! (normal mode) 
        HDA_codec_query(codec, node_id, VERB_SET_POWER_STATE);
    }
}


int HDA_codec_list_widgets(uint8_t codec){
    uint32_t parameter;
    uint8_t func_grp_count;
    uint8_t init_func_grp_num;
    uint32_t i;
    uint32_t j;
    uint8_t widget_count;
    uint8_t init_widget_num;
    parameter = HDA_codec_query(codec, 0, VERB_GET_PARAMETER | PARAM_FUNCTION_GROUP_TYPE);
    printf("Function Group Type: 0x%x\n", parameter);
    parameter = HDA_codec_query(codec, 0, VERB_GET_PARAMETER | PARAM_SUB_NODE_COUNT);
    
    printf("Function group number: 0x%x\n", parameter);

    //lower 8 bits = number of subnodes for this codec
    func_grp_count = (0xFF & parameter);
    //TODO: Why are all responses 0?
    init_func_grp_num = (0xFF & (parameter >> 16));
    /*
    Do the above over again for all found sub nodes.
    peep the group type.?
    */
   printf("1\n");
    for(i = 0; i < func_grp_count; i++){
        parameter = HDA_codec_query(codec, init_func_grp_num + i, VERB_GET_PARAMETER | PARAM_SUB_NODE_COUNT);
        widget_count = parameter & 0xFF; // lower 8 bits
        init_widget_num = (parameter >> 16) & 0xFF; // bits 23-16
        printf("Initial Widget Number: %d\n", init_widget_num);
        //parameter = HDA_codec_query(codec, init_func_grp_num + i, VERB_SET_POWER_STATE | 0x0);
        
        for(j = 0; j < init_widget_num; j++){
            HDA_widget_init(codec,  init_widget_num + j);
        }
    }
    printf("2\n");

    if(audio_device.output.node_id){
        return 0;
    }
    else{
        printf("3\n");

        return -1;
    }
}

void HDA_list_codecs(){
    
    uint16_t statests_reg = hda_reg_readword(STATESTS);
    uint8_t i;
    // for(i = 0; i < 100; i += 2){
        
    //     printf("i = 0x%x    0x%x",i,hda_reg_readlong(i));
    //     if(i % 10 == 0){rtc_read(0,0,0);rtc_read(0,0,0);rtc_read(0,0,0);}
    // }
    printf("\nSTATESTS_REG: 0x%x\n", statests_reg);
    //Don't have enoguh space to support many devices. just choose first.
    for(i = 0; i < SDIN_LEN; i++){
        if(statests_reg & (1 << i)){
            HDA_codec_list_widgets(i);
                return;
        }
    }
}

void HDA_reset(){
    //Clear out CTL bits for CORB and RIRB first
    //Reference code writes to 4 registers instead of just the control register.
    //weird. i guess it doesn't matter since they'll be overwritten anyways?
    hda_reg_writelong(CORBCTL, 0x0); 
    hda_reg_writelong(RIRBCTL, 0x0);
    /* Wait for the DMA engines to be turned off*/
    while((hda_reg_readlong(CORBCTL) & CORBCTL_CORBRUN) | (hda_reg_readword(RIRBCTL) & RIRBCTL_RIRBRUN))
    {/* Wait . . . */};
    /* Done waiting! */
    
    //reset global control reg
    hda_reg_writelong(GCTL, 0x0);
    while(hda_reg_readlong(GCTL) & CRST)
    {/* Wait . . . */};
    hda_reg_writelong(GCTL, CRST);
    while((hda_reg_readlong(GCTL) & CRST) == 0)
    {/* Wait . . . */}
    //Enable all interrupts
    hda_reg_writeword(WAKEEN, 0xFFFF);
    printf("\nGCAP: 0x%x\n",hda_reg_readword(0));
    printf("WAKEEN: 0x%x\n",hda_reg_readword(WAKEEN));
    printf("\nreset_finished!\n");

    printf("Start of codec \n");
    hda_reg_writelong(INTCTL, 0x800003F);
    
    HDA_corb_init();
    HDA_rirb_init();
    rtc_read(0,0,0);
    HDA_list_codecs();

}
void HDA_set_default_volume(){
    HDA_set_volume(255); // max
};
void HDA_init_stream_descriptor(){
    // uint32_t bdl_base, dma_pos_base;
    // uint32_t i;
    // hda_reg_writebyte(0x102, 0x10);
    // hda_reg_writelong(0x108, 4*BUFFER_SIZE);
    // hda_reg_writeword(0x10c, 3);
    // bdl_base = HDA_MEMORY_LOCATION + 3072;
    // hda_reg_writelong(0x118, bdl_base);

    // for(i = 0; i < 4; i++){
    //     audio_device.bdl[i] = audio_device.buffer[i] + (i * BUFFER_SIZE);
    //     audio_device.bdl[i].length = BUFFER_SIZE;
    //     audio_device.bdl[i].flags = 1;
    // }

    // //TODO: memset to 0??
    // for(i = 0; i < 8; i++){
    //     audio_device.dma_pos[i] = 0;
    // }
    // dma_pos_base = HDA_MEMORY_LOCATION + 3072 + ((4 * sizeof(struct hda_bdl_entry) + 127) & ~127);
    // hda_reg_writelong(0x70, dma_pos_base | 1);


}
void HDA_init_dev(){

    pci_hda_dev = PCI_get_device_by_ID(0x2668);
    
    uint16_t command_val = PCI_config_readword(pci_hda_dev.bus, pci_hda_dev.slot, pci_hda_dev.fn, REGISTER_1_OFFSET + WORD_0_OFFSET);
    //enable io/busmastering/memspace
    PCI_config_writeword(pci_hda_dev.bus, pci_hda_dev.slot, pci_hda_dev.fn, REGISTER_1_OFFSET + WORD_0_OFFSET, command_val | 0x7);
    PCI_config_writelong(pci_hda_dev.bus, pci_hda_dev.slot, pci_hda_dev.fn, REGISTER_4_OFFSET, HDA_MEMORY_LOCATION);
    PCI_print_device(pci_hda_dev.bus, pci_hda_dev.slot);
    audio_device.rirb_read_pointer = 0;
    audio_device.buffer = malloc(1);
    HDA_reset();
    //TODO: the below.
    HDA_init_out_widget();
    HDA_init_stream_descriptor();
    
    HDA_set_default_volume();
}

void HDA_set_volume(uint8_t vol){
    return;
}
// void HDA_set_sampling_rate(){}
// void HDA_set_channels(){}

// void HDA_send_data(){}

// void HDA_get_status(){}
// void HDA_set_status(){}

// void HDA_get_position(){}
// void HDA_set_position(){}
