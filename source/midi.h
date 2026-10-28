#ifndef GEEZY_MIDI_H
#define GEEZY_MIDI_H
#include "controls.h"
#include <string.h>
#include <stddef.h>

typedef struct { uint8_t in,out,interface,alternate,configuration; uint16_t in_size,out_size; } MidiEndpoints;
// Parse each alternate interface independently; never pair endpoints from different interfaces.
static inline int midi_endpoints(const uint8_t *data,size_t size,MidiEndpoints *out) {
    if(size<9 || data[0]<9 || data[1]!=2) return 0;
    unsigned total=data[2]|data[3]<<8;
    if(total>size || total<9) return 0;
    MidiEndpoints candidate={.configuration=data[5]}; int streaming=0;
    for(size_t i=data[0];i<total;) {
        unsigned length=data[i];
        if(length<2 || i+length>total) return 0;
        if(data[i+1]==4) {
            if(length<9) return 0;
            candidate=(MidiEndpoints){.configuration=data[5],.interface=data[i+2],.alternate=data[i+3]};
            streaming=data[i+5]==1 && data[i+6]==3 && data[i+7]==0;
        } else if(streaming && data[i+1]==5) {
            if(length<7) return 0;
            unsigned packet=data[i+4]|data[i+5]<<8;
            if((data[i+3]&3)==2 && packet>=4 && packet<=512 && packet%4==0) {
                if(data[i+2]&0x80) { candidate.in=data[i+2]; candidate.in_size=packet; }
                else { candidate.out=data[i+2]; candidate.out_size=packet; }
            }
            if(candidate.in && candidate.out) { *out=candidate; return 1; }
        }
        i+=length;
    }
    return 0;
}
static inline int midi_note(const uint8_t packet[4],int *note,int *on) {
    unsigned cin=packet[0]&15,status=packet[1]&0xf0;
    if(packet[0]>>4 || packet[2]>=128 || packet[3]>=128) return 0;
    if(!((cin==9 && status==0x90)||(cin==8 && status==0x80))) return 0;
    *note=packet[2]; *on=status==0x90 && packet[3]!=0; return 1;
}
static inline int midi_normal_note(int note,int mk2) {
    if(mk2 && note>=100 && note<=107) return note-100+64;
    if(mk2 && note>=112 && note<=119) return note-112+82;
    return note;
}
static inline int midi_wire_note(int note,int mk2) {
    if(mk2 && note>=64 && note<=71) return note-64+100;
    if(mk2 && note>=82 && note<=89) return note-82+112;
    return note;
}
static inline void midi_led_packet(uint8_t out[4],int note,int color,int mk2) {
    out[0]=9; out[1]=(mk2 && note<64)?0x96:0x90;
    out[2]=midi_wire_note(note,mk2);
    out[3]=note>=64?(color?1:0):mk2?(color==1?21:color==3?13:color==5?5:0):color;
}
typedef struct { uint8_t held[128]; unsigned order[128],serial; } MidiInput;
static inline int midi_action(Synth *s,MidiInput *input,int note,int on) {
    if(note<0 || note>=128 || input->held[note]==on) return 0;
    input->held[note]=on;
    if(!on) return 0;
    input->order[note]=++input->serial;
    if(note>=64 && note<68) { theme_preset(s,note-64); return 1; }
    if(note>=68 && note<72) { s->gear=note-68; return 0; }
    if(note>=72 && note<76) { s->motion_mode=note-72; return 0; }
    if(note>=76 && note<80) { space_preset(s,note-76); return 0; }
    if(note==80) { remix(s); return 1; }
    if(note==81) { s->hud_visible=!s->hud_visible; return 0; }
    if(note>=82 && note<=89) return 0;
    if(note>=56 && note<=60) { s->world_mode=note-56; return 0; }
    if(note==61) { s->theme=(s->theme+1)%4; return 1; }
    if(note==62) { s->skin=(s->skin+1)%4; return 0; }
    if(note==63) { s->reflection=(s->reflection+1)%4; return 1; }
    if(note>=48 && note<=55) { space_preset(s,note-48); return 0; }
    if(note>=40 && note<=47) { object_preset(s,note-40); return 0; }
    if(note>=36 && note<=39) { s->gear=note-36; return 0; }
    if(note>=32 && note<=35) { s->motion_mode=note-32; s->motion_phase=s->grid_distance=0; s->grid_tick=0; return 0; }
    if(note>=16 && note<=31) { harmony_bank(s,note-16); return 1; }
    if(note>=8 && note<=15) { s->mask_pattern=note-8; return 0; }
    if(note<4) { steer(s,(int[]){-1,0,0,1}[note],(int[]){0,-1,1,0}[note]); hit(s,1.5f); }
    if(note==5) camera_cut(s);
    if(note==6) { remix(s); return 1; }
    if(note==7) s->hud_visible=!s->hud_visible;
    return 0;
}
static inline int midi_overlay(const MidiInput *input) {
    unsigned latest=0; int pattern=-1;
    for(int n=8;n<=15;n++) if(input->held[n] && input->order[n]>latest) {
        latest=input->order[n]; pattern=n-8;
    }
    return pattern;
}
static inline unsigned midi_master_fx(const MidiInput *input) {
    unsigned fx=0; for(int i=0;i<8;i++) if(input->held[82+i]) fx|=1u<<i;
    return fx;
}
static inline int midi_steering(Synth *s,const MidiInput *input) {
    int x=input->held[3]-input->held[0],y=input->held[2]-input->held[1];
    if(x||y) { steer(s,x,y); return 1; } return 0;
}
static inline void midi_leds(const Synth *s,const MidiInput *input,uint8_t leds[90]) {
    memset(leds,0,90);
    for(int i=0;i<8;i++) {
        Synth p=*s; space_preset(&p,i); if(s->backdrop==p.backdrop && s->beam_style==p.beam_style) leds[48+i]=1;
        p=*s; object_preset(&p,i); if(s->shape==p.shape && s->layout==p.layout) leds[40+i]=1;
    }
    leds[32+s->motion_mode]=leds[36+s->gear]=leds[16+s->palette_bank]=1;
    leds[64+s->theme]=leds[68+s->gear]=1;
    for(int n=0;n<90;n++) if(input->held[n]) leds[n]=(n==4||n==5)?5:3;
    for(int n=82;n<=89;n++) leds[n]=input->held[n]?1:0;
    leds[56+s->world_mode]=1;
}

#ifdef HW_RVL
#include <gccore.h>
#include <ogc/usb.h>
#include <ogc/irq.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
// Enumeration/control calls can block in IOS. Only this low-priority worker calls USB APIs.
static struct {
    volatile int stop,connected,fault,read_pending,write_pending;
    volatile int read_result,write_result;
    unsigned generation,head,tail;
    struct { uint8_t note,on; } events[256];
    uint8_t wanted[90];
} midi_usb;
static uint8_t midi_rx[512] ATTRIBUTE_ALIGN(32),midi_tx[512] ATTRIBUTE_ALIGN(32);
static uint8_t midi_descriptor[4096] ATTRIBUTE_ALIGN(32);
static uint8_t midi_stack[16384] ATTRIBUTE_ALIGN(32);
static lwp_t midi_thread;
static s32 midi_read_done(s32 result,void *arg) { (void)arg; midi_usb.read_result=result; midi_usb.read_pending=0; return 0; }
static s32 midi_write_done(s32 result,void *arg) { (void)arg; midi_usb.write_result=result; midi_usb.write_pending=0; return 0; }
static s32 midi_removed(s32 result,void *arg) { (void)result; (void)arg; midi_usb.fault=1; return 0; }
static void midi_forget(void) {
    u32 irq=IRQ_Disable(); midi_usb.connected=0; midi_usb.generation++;
    midi_usb.head=midi_usb.tail=0; IRQ_Restore(irq);
}
static void *midi_worker(void *arg) {
    (void)arg;
    if(USB_Initialize()<0) return NULL;
    while(!midi_usb.stop) {
        usb_device_entry devices[32]; u8 count=0; s32 fd=-1; MidiEndpoints ep={0}; int mk2=0,leds_enabled=0;
        if(USB_GetDeviceList(devices,32,0,&count)>=0) for(int i=0;i<count && fd==-1;i++) {
            s32 candidate=-1;
            if(USB_OpenDevice(devices[i].device_id,devices[i].vid,devices[i].pid,&candidate)<0) continue;
            usb_devdesc desc; memset(&desc,0,sizeof(desc));
            int found=0;
            if(USB_GetDeviceDescription(candidate,&desc)>=0) for(int c=0;c<desc.bNumConfigurations && !found;c++) {
                int n=USB_ReadCtrlMsg(candidate,0x80,USB_REQ_GETDESCRIPTOR,(USB_DT_CONFIG<<8)|c,0,9,midi_descriptor);
                if(n<9) continue;
                unsigned length=midi_descriptor[2]|midi_descriptor[3]<<8;
                if(length<9 || length>sizeof(midi_descriptor)) continue;
                n=USB_ReadCtrlMsg(candidate,0x80,USB_REQ_GETDESCRIPTOR,(USB_DT_CONFIG<<8)|c,0,length,midi_descriptor);
                found=n>0 && midi_endpoints(midi_descriptor,n,&ep);
            }
            if(found) {
                u8 configuration=0;
                if(USB_GetConfiguration(candidate,&configuration)<0 || (configuration!=ep.configuration && USB_SetConfiguration(candidate,ep.configuration)<0)) found=0;
                if(found && ep.alternate && USB_SetAlternativeInterface(candidate,ep.interface,ep.alternate)<0) found=0;
            }
            if(found) {
                fd=candidate; leds_enabled=devices[i].vid==0x09e8 && devices[i].pid==0x0028;
                // Product string also covers revisions whose USB PID differs from the SysEx model ID.
                memset(midi_descriptor,0,sizeof(midi_descriptor));
                if(desc.iProduct && USB_GetAsciiString(fd,desc.iProduct,0x0409,128,midi_descriptor)>=0) {
                    for(int c=0;c<127;c++) if(midi_descriptor[c]>='A' && midi_descriptor[c]<='Z') midi_descriptor[c]+=32;
                    midi_descriptor[127]=0;
                    int apc=strstr((char*)midi_descriptor,"apc") && strstr((char*)midi_descriptor,"mini");
                    leds_enabled|=apc;
                    mk2=apc && (strstr((char*)midi_descriptor,"mk2") || strstr((char*)midi_descriptor,"mkii"));
                }
            } else USB_CloseDevice(&candidate);
        }
        if(fd==-1) { usleep(500000); continue; }
        midi_usb.fault=0; midi_usb.read_result=midi_usb.write_result=0;
        midi_usb.connected=1;
        USB_DeviceRemovalNotifyAsync(fd,midi_removed,NULL);
        uint8_t sent[90],batch_notes[90],batch_values[90]; memset(sent,255,sizeof(sent));
        int batch=0,write_length=0; u64 write_start=0;
        while(!midi_usb.stop && !midi_usb.fault) {
            if(!midi_usb.read_pending) {
                int n=midi_usb.read_result; midi_usb.read_result=0;
                if(n<0 || n>ep.in_size) break;
                DCInvalidateRange(midi_rx,sizeof(midi_rx));
                for(int i=0;i+4<=n;i+=4) {
                    int note,on;
                    if(!midi_note(midi_rx+i,&note,&on)) continue;
                    note=midi_normal_note(note,mk2);
                    u32 irq=IRQ_Disable(); unsigned next=(midi_usb.head+1)%256;
                    if(next==midi_usb.tail) { midi_usb.generation++; midi_usb.tail=midi_usb.head=0; }
                    else { midi_usb.events[midi_usb.head].note=note; midi_usb.events[midi_usb.head].on=on; midi_usb.head=next; }
                    IRQ_Restore(irq);
                }
                midi_usb.read_pending=1;
                if(USB_ReadBlkMsgAsync(fd,ep.in,ep.in_size,midi_rx,midi_read_done,NULL)<0) { midi_usb.read_pending=0; break; }
            }
            if(leds_enabled && !midi_usb.write_pending) {
                if(batch && midi_usb.write_result!=write_length) break;
                for(int i=0;i<batch;i++) sent[batch_notes[i]]=batch_values[i];
                batch=0;
                u32 irq=IRQ_Disable(); uint8_t wanted[90]; memcpy(wanted,midi_usb.wanted,90); IRQ_Restore(irq);
                for(int note=0;note<90 && (batch+1)*4<=ep.out_size;note++) {
                    if(note>=72 && note<82) continue; // No physical APC LEDs at these logical aliases.
                    if(sent[note]==wanted[note]) continue;
                    batch_notes[batch]=note; batch_values[batch]=wanted[note];
                    midi_led_packet(midi_tx+batch*4,note,wanted[note],mk2); batch++;
                }
                if(batch) {
                    write_length=batch*4; midi_usb.write_pending=1; write_start=gettime();
                    DCFlushRange(midi_tx,sizeof(midi_tx));
                    if(USB_WriteBlkMsgAsync(fd,ep.out,write_length,midi_tx,midi_write_done,NULL)<0) { midi_usb.write_pending=0; break; }
                }
            } else if(midi_usb.write_pending && ticks_to_millisecs(gettime()-write_start)>2000) break;
            usleep(2000);
        }
        midi_forget(); USB_ClearHalt(fd,ep.in); USB_ClearHalt(fd,ep.out); USB_CloseDevice(&fd);
        // Never reuse DMA buffers until cancellation callbacks have relinquished them.
        while((midi_usb.read_pending || midi_usb.write_pending) && !midi_usb.stop) usleep(10000);
        usleep(500000);
    }
    return NULL;
}
static void midi_start(void) { LWP_CreateThread(&midi_thread,midi_worker,NULL,midi_stack,sizeof(midi_stack),40); }
static int midi_poll(Synth *s,MidiInput *input) {
    static unsigned generation; int dirty=0;
    for(int i=0;i<256;i++) {
        u32 irq=IRQ_Disable();
        if(generation!=midi_usb.generation) { generation=midi_usb.generation; memset(input,0,sizeof(*input)); }
        if(midi_usb.tail==midi_usb.head) { IRQ_Restore(irq); break; }
        int note=midi_usb.events[midi_usb.tail].note,on=midi_usb.events[midi_usb.tail].on;
        midi_usb.tail=(midi_usb.tail+1)%256; IRQ_Restore(irq);
        dirty|=midi_action(s,input,note,on);
    }
    uint8_t leds[90]; midi_leds(s,input,leds);
    u32 irq=IRQ_Disable(); memcpy(midi_usb.wanted,leds,90); IRQ_Restore(irq);
    return dirty;
}
#else
static struct { int stop; } midi_usb;
static inline void midi_start(void) { midi_usb.stop=0; }
static inline int midi_poll(Synth *s,MidiInput *input) { (void)s; (void)input; return 0; }
#endif
#endif
