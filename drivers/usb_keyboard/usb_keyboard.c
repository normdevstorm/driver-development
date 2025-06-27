#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/slab.h>

#define DRIVER_NAME "usb_keyboard_driver"
#define DRIVER_VERSION "1.0"

/* USB keyboard data structure */
struct usb_keyboard {
    struct input_dev *input_dev;
    struct usb_device *udev;
    struct usb_interface *interface;
    struct urb *irq_urb;
    unsigned char *data;
    dma_addr_t data_dma;
    int data_size;
    
    char name[128];
    char phys[64];
};

/* Standard USB keyboard key codes */
static const unsigned char usb_kbd_keycode[256] = {
      0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
     50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
      4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
     27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
     65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
    105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
     72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
    191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
    115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
    122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
    150,158,159,128,136,177,178,176,142,152,173,140
};

/* USB keyboard interrupt handler */
static void usb_kbd_irq(struct urb *urb)
{
    struct usb_keyboard *kbd = urb->context;
    unsigned char *data = kbd->data;
    struct input_dev *input = kbd->input_dev;
    int i;
    
    switch (urb->status) {
    case 0:            /* success */
        break;
    case -ECONNRESET:  /* unlink */
    case -ENOENT:
    case -ESHUTDOWN:
        return;
    default:           /* error */
        printk(KERN_DEBUG "usb_keyboard: URB error %d\n", urb->status);
        goto resubmit;
    }
    
    /* Process modifier keys (Ctrl, Alt, Shift, etc.) */
    input_report_key(input, KEY_LEFTCTRL,   data[0] & 0x01);
    input_report_key(input, KEY_LEFTSHIFT,  data[0] & 0x02);
    input_report_key(input, KEY_LEFTALT,    data[0] & 0x04);
    input_report_key(input, KEY_LEFTMETA,   data[0] & 0x08);
    input_report_key(input, KEY_RIGHTCTRL,  data[0] & 0x10);
    input_report_key(input, KEY_RIGHTSHIFT, data[0] & 0x20);
    input_report_key(input, KEY_RIGHTALT,   data[0] & 0x40);
    input_report_key(input, KEY_RIGHTMETA,  data[0] & 0x80);
    
    /* Process regular key presses (data[2] to data[7]) */
    for (i = 2; i < 8; i++) {
        if (data[i] > 3 && usb_kbd_keycode[data[i]]) {
            input_report_key(input, usb_kbd_keycode[data[i]], 1);
            printk(KERN_INFO "usb_keyboard: Key pressed: 0x%02x -> %d\n", 
                   data[i], usb_kbd_keycode[data[i]]);
        }
    }
    
    /* Report key releases by comparing with previous state */
    for (i = 2; i < 8; i++) {
        if (data[i] == 0) {
            /* Key released - would need to track previous state */
            continue;
        }
    }
    
    input_sync(input);
    
resubmit:
    if (usb_submit_urb(urb, GFP_ATOMIC))
        printk(KERN_ERR "usb_keyboard: Failed to resubmit URB\n");
}

/* USB keyboard probe function */
static int usb_kbd_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_keyboard *kbd;
    struct input_dev *input_dev;
    struct usb_endpoint_descriptor *endpoint;
    struct usb_host_interface *iface_desc;
    int i, pipe, maxp;
    int error = -ENOMEM;
    
    printk(KERN_INFO "usb_keyboard: USB keyboard detected\n");
    
    /* Find the interrupt IN endpoint */
    iface_desc = interface->cur_altsetting;
    endpoint = NULL;
    
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        struct usb_endpoint_descriptor *ep = &iface_desc->endpoint[i].desc;
        
        if (usb_endpoint_is_int_in(ep)) {
            endpoint = ep;
            break;
        }
    }
    
    if (!endpoint) {
        printk(KERN_ERR "usb_keyboard: No interrupt IN endpoint found\n");
        return -ENODEV;
    }
    
    pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
    maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));
    
    /* Allocate keyboard structure */
    kbd = kzalloc(sizeof(struct usb_keyboard), GFP_KERNEL);
    if (!kbd)
        return -ENOMEM;
    
    /* Allocate input device */
    input_dev = input_allocate_device();
    if (!input_dev)
        goto fail1;
    
    /* Allocate data buffer */
    kbd->data = usb_alloc_coherent(udev, 8, GFP_ATOMIC, &kbd->data_dma);
    if (!kbd->data)
        goto fail2;
    
    /* Allocate URB */
    kbd->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!kbd->irq_urb)
        goto fail3;
    
    /* Initialize keyboard structure */
    kbd->udev = udev;
    kbd->interface = interface;
    kbd->input_dev = input_dev;
    kbd->data_size = 8;
    
    /* Set up device name and physical location */
    if (udev->manufacturer)
        strlcpy(kbd->name, udev->manufacturer, sizeof(kbd->name));
    
    if (udev->product) {
        if (udev->manufacturer)
            strlcat(kbd->name, " ", sizeof(kbd->name));
        strlcat(kbd->name, udev->product, sizeof(kbd->name));
    }
    
    if (!strlen(kbd->name))
        snprintf(kbd->name, sizeof(kbd->name), "USB Keyboard %04x:%04x",
                 le16_to_cpu(udev->descriptor.idVendor),
                 le16_to_cpu(udev->descriptor.idProduct));
    
    usb_make_path(udev, kbd->phys, sizeof(kbd->phys));
    strlcat(kbd->phys, "/input0", sizeof(kbd->phys));
    
    /* Configure input device */
    input_dev->name = kbd->name;
    input_dev->phys = kbd->phys;
    usb_to_input_id(udev, &input_dev->id);
    input_dev->dev.parent = &interface->dev;
    
    input_set_drvdata(input_dev, kbd);
    
    /* Set supported event types */
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
    
    /* Set supported keys */
    for (i = 0; i < 255; i++)
        if (usb_kbd_keycode[i])
            __set_bit(usb_kbd_keycode[i], input_dev->keybit);
    
    /* Set modifier keys */
    __set_bit(KEY_LEFTCTRL, input_dev->keybit);
    __set_bit(KEY_LEFTSHIFT, input_dev->keybit);
    __set_bit(KEY_LEFTALT, input_dev->keybit);
    __set_bit(KEY_LEFTMETA, input_dev->keybit);
    __set_bit(KEY_RIGHTCTRL, input_dev->keybit);
    __set_bit(KEY_RIGHTSHIFT, input_dev->keybit);
    __set_bit(KEY_RIGHTALT, input_dev->keybit);
    __set_bit(KEY_RIGHTMETA, input_dev->keybit);
    
    /* Initialize URB */
    usb_fill_int_urb(kbd->irq_urb, udev, pipe, kbd->data, 8,
                     usb_kbd_irq, kbd, endpoint->bInterval);
    kbd->irq_urb->transfer_dma = kbd->data_dma;
    kbd->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    
    /* Register input device */
    error = input_register_device(kbd->input_dev);
    if (error)
        goto fail4;
    
    /* Submit URB */
    error = usb_submit_urb(kbd->irq_urb, GFP_KERNEL);
    if (error)
        goto fail5;
    
    /* Store keyboard in interface data */
    usb_set_intfdata(interface, kbd);
    
    printk(KERN_INFO "usb_keyboard: %s initialized\n", kbd->name);
    return 0;
    
fail5:
    input_unregister_device(input_dev);
    input_dev = NULL;
fail4:
    usb_free_urb(kbd->irq_urb);
fail3:
    usb_free_coherent(udev, 8, kbd->data, kbd->data_dma);
fail2:
    input_free_device(input_dev);
fail1:
    kfree(kbd);
    return error;
}

/* USB keyboard disconnect function */
static void usb_kbd_disconnect(struct usb_interface *interface)
{
    struct usb_keyboard *kbd = usb_get_intfdata(interface);
    
    printk(KERN_INFO "usb_keyboard: USB keyboard disconnected\n");
    
    usb_set_intfdata(interface, NULL);
    if (kbd) {
        usb_kill_urb(kbd->irq_urb);
        input_unregister_device(kbd->input_dev);
        usb_free_urb(kbd->irq_urb);
        usb_free_coherent(kbd->udev, kbd->data_size, kbd->data, kbd->data_dma);
        kfree(kbd);
    }
}

/* USB device ID table */
static struct usb_device_id usb_kbd_id_table[] = {
    { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, 
                        USB_INTERFACE_SUBCLASS_BOOT,
                        USB_INTERFACE_PROTOCOL_KEYBOARD) },
    { }
};

MODULE_DEVICE_TABLE(usb, usb_kbd_id_table);

/* USB driver structure */
static struct usb_driver usb_kbd_driver = {
    .name       = DRIVER_NAME,
    .probe      = usb_kbd_probe,
    .disconnect = usb_kbd_disconnect,
    .id_table   = usb_kbd_id_table,
};

/* Module initialization */
static int __init usb_kbd_init(void)
{
    int result;
    
    printk(KERN_INFO "usb_keyboard: Initializing USB keyboard driver v%s\n", 
           DRIVER_VERSION);
    
    result = usb_register(&usb_kbd_driver);
    if (result)
        printk(KERN_ERR "usb_keyboard: Failed to register USB driver\n");
    else
        printk(KERN_INFO "usb_keyboard: USB keyboard driver registered\n");
    
    return result;
}

/* Module cleanup */
static void __exit usb_kbd_exit(void)
{
    usb_deregister(&usb_kbd_driver);
    printk(KERN_INFO "usb_keyboard: USB keyboard driver unregistered\n");
}

module_init(usb_kbd_init);
module_exit(usb_kbd_exit);

MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("USB Keyboard Driver for CentOS 32-bit");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
