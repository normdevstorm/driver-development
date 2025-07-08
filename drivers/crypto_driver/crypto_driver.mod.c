#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_MITIGATION_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x4ae3a929, "class_create" },
	{ 0xf87d1e67, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x6fb70485, "device_create" },
	{ 0x13d1c90f, "class_destroy" },
	{ 0xcd4a16cb, "crypto_alloc_skcipher" },
	{ 0x7956cc9e, "device_destroy" },
	{ 0x8719d347, "crypto_alloc_shash" },
	{ 0x43c5517f, "crypto_destroy_tfm" },
	{ 0x13bf37b7, "crypto_skcipher_setkey" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x43babd19, "sg_init_one" },
	{ 0xaf40f205, "crypto_skcipher_decrypt" },
	{ 0xd0760fc0, "kfree_sensitive" },
	{ 0x37a0cba, "kfree" },
	{ 0x398ab06, "crypto_skcipher_encrypt" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x41ed3709, "get_random_bytes" },
	{ 0x69ef14bf, "kmalloc_caches" },
	{ 0x817f91ba, "kmalloc_trace" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xfe4785cd, "crypto_shash_update" },
	{ 0xf16d7b00, "crypto_shash_final" },
	{ 0x7682ba4e, "__copy_overflow" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x92997ed8, "_printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x69acdf38, "memcpy" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xdbc542c2, "cdev_init" },
	{ 0x4c74e78f, "cdev_add" },
	{ 0x7d196a5f, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "E3E2F1400E5095D2B268EFF");
MODULE_INFO(rhelversion, "9.7");
