#include <linux/module.h>     /* Для всех модулей */
#include <linux/kernel.h>     /* KERN_INFO */
#include <linux/init.h>       /* Макросы */
#include <linux/fs.h>         /* Макросы для устройств */
#include <linux/cdev.h>       /* Функции регистрации символьных устройств */
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Makar Selivanov");
MODULE_DESCRIPTION("This module create buffers in memory");
MODULE_VERSION("1.0");

#define MAX_COUNT 20
static struct semaphore buffer_lock[MAX_COUNT];
static char *buffer[MAX_COUNT];
static size_t buffer_size[MAX_COUNT];
static size_t device_usage[MAX_COUNT];

static size_t size = 1024;
module_param(size, ulong, 0644);
MODULE_PARM_DESC(size, "Default size of buffer");

//TODO add something to change size of already existing buffers

static size_t count = 0;

int param_count_set(const char *val, const struct kernel_param *kp)
{
    int ncount, res;

    if (kstrtoint(val, 0, &ncount))
    {
        pr_err("BUFFER: bad count format");
        return -ERANGE;
    }

    if (ncount > MAX_COUNT)
    {
        pr_err("BUFFER: max count exceeded, should be <%d, given %d\n", MAX_COUNT, ncount);
        return -ERANGE;
    }
    ncount = count;
    res = param_set_int(val, kp);
    if (res == 0) 
    {
        if (ncount < count) {
            //Old was less than new, need to create new devices
            while (ncount < count) {
                buffer_size[ncount] = size;
                device_usage[ncount] = 0;
                buffer[ncount] = kzalloc(buffer_size[ncount], GFP_KERNEL);
                ++ncount;
            }
        }
        else if (ncount > count) {
            //Old was bigger than new, need to delete
            while (ncount > count) {
                down(buffer_lock + ncount); //lock
                kfree(buffer[ncount]);
                buffer[ncount] = 0;
                up(buffer_lock + ncount);  //unlock
                --ncount;
            }
        } else {
            pr_info("BUFFER: count don't change\n");
        }
        pr_info("BUFFER: new count = %lu\n", count);
        return 0;
    }

    pr_err("BUFFER: unknown error\n");
    return -ERANGE;
}
const struct kernel_param_ops param_count_of = 
{
    .set = &param_count_set,
    .get = &param_get_ulong,
};

module_param_cb(count, &param_count_of, &count, 0644);
MODULE_PARM_DESC(count, "Max count of devices");

static ssize_t chrdev_read(struct file *file, char __user *buf, size_t length, loff_t *off)
{
    size_t offset = *off;
    ssize_t uncopy;
    unsigned int minor = (uintptr_t) file->private_data;
    pr_info("BUFFER: device %u, reader with offset %lld and length %ld\n", minor, *off, length);

    down(buffer_lock + minor);
    if (length + offset > buffer_size[minor])
        length = buffer_size[minor] - offset;
    uncopy = copy_to_user(buf, buffer[minor] + offset, length);
    up(buffer_lock + minor);
    if (uncopy > 0) {
        pr_err("BUFFER: copy_to_user failed, doesn't copy %lu bytes", uncopy);
        return -EFAULT;
    }
    *off += length;
    return length;
}

static ssize_t chrdev_write(struct file *file, const char __user *buf, size_t length, loff_t *off)
{
    size_t offset = 0;
    ssize_t uncopy;
    char *tmp_buffer;
    unsigned int minor = (uintptr_t) file->private_data;
    pr_info("BUFFER: device %u, writing with offset %lld and length %lu\n", minor, *off, length);
    if (*off >= buffer_size[minor])
        return -EINVAL;

    if (off != NULL)
        offset = *off;

    tmp_buffer = kzalloc(length, GFP_KERNEL);
    if (tmp_buffer == NULL) {
        pr_err("BUFFER: can't alloc memory for tmp buffer");
        return -ENOMEM;
    }
    down(buffer_lock + minor);
    if (length + offset > buffer_size[minor])
        length = buffer_size[minor] - offset;
    uncopy = copy_from_user(tmp_buffer, buf, length);
    up(buffer_lock + minor);
    if (uncopy > 0) {
        pr_err("BUFFER: copy_from_user failed, doesn't copy %lu bytes", uncopy);
        kfree(tmp_buffer);
        return -EFAULT;
    }
    memcpy(buffer[minor] + offset, tmp_buffer, length);
    *off += length;
    return length;
}

static int chrdev_open(struct inode *inode, struct file *file) {
    int minor = iminor(inode);

    if (device_usage[minor] == 0) {
        buffer[minor] = kzalloc(size, GFP_KERNEL);
        buffer_size[minor] = size;
        if (buffer[minor] == NULL) {    
            pr_err("BUFFER: can't init %d device buffer", minor);
            //TODO remove all buffers?
        }
    }
    ++device_usage[minor];
    pr_info("BUFFER: open %d device, current usage: %lu", minor, device_usage[minor]);
    file->private_data = (void *) (uintptr_t) minor;
    return 0;
}

static int chrdev_release(struct inode *inode, struct file *file) {
    int minor = iminor(inode);
    --device_usage[minor];
    pr_info("BUFFER: close %d device, current usage: %lu", minor, device_usage[minor]);
    if (device_usage[minor] == 0) {
        kfree(buffer[minor]);
        buffer[minor] = 0;
    }
    return 0;
}

static struct file_operations chrdev_ops =
{
    .owner      = THIS_MODULE,
    .read       = chrdev_read,
    .write      = chrdev_write,
    .open       = chrdev_open,
    .release    = chrdev_release
};

dev_t dev = 0;
static struct cdev chrdev_cdev;
static struct class *chrdev_class;


static int __init module_start(void)
{
    int i, res;
    for (i = 0; i < MAX_COUNT; ++i) {
        device_usage[i] = 0;
        buffer[i] = 0;
        sema_init(buffer_lock + i, 1);
    }
    if (count > MAX_COUNT)
    {
        pr_err("BUFFER: max count exceeded, should be < %d, given %lu\n", MAX_COUNT, count);
        return -ERANGE;
    }
    pr_info("BUFFER: load size=%lu count=%lu\n", size, count);
    if ((res = alloc_chrdev_region(&dev, 0, MAX_COUNT, "chrdev")) < 0) {
        pr_err("Error allocating major number\n");
        return res;
    }
    pr_info("CHRDEV load: Major = %d Minor = %d\n", MAJOR(dev), MINOR(dev));
    cdev_init(&chrdev_cdev, &chrdev_ops);
    if ((res = cdev_add(&chrdev_cdev, dev, count)) < 0) {
        pr_err("CHRDEV: device registering error\n");
        unregister_chrdev_region(dev, 1);
        return res;
    }

    if (IS_ERR(chrdev_class = class_create(THIS_MODULE, "buffer_class"))) {
        cdev_del(&chrdev_cdev);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    char str[64];
    for (int i = 0; i < count; ++i) {
        sprintf(str, "buffer_device_%d", i);
        if (IS_ERR(device_create(chrdev_class, NULL, dev, NULL, str))) {
            pr_err("CHRDEV: error creating device\n");
            class_destroy(chrdev_class);
            cdev_del(&chrdev_cdev);
            unregister_chrdev_region(dev, 1);
            return -1;
        }
    }
    return 0;
}

static void __exit module_end(void)
{
    int i;
    device_destroy (chrdev_class, dev);
    class_destroy (chrdev_class);
    cdev_del (&chrdev_cdev);
    unregister_chrdev_region(dev, 1);
    for (i = 0; i < MAX_COUNT; i++)
        kfree(buffer[i]);
    pr_info("BUFFER: unload");
}

module_init(module_start);
module_exit(module_end);