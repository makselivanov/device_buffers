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

#define TRUE 1
#define FALSE 0

#define MAX_COUNT 20
static struct rw_semaphore buffer_lock[MAX_COUNT];
static char *buffer[MAX_COUNT];
static size_t buffer_size[MAX_COUNT];
static size_t device_usage[MAX_COUNT];
static int chrdev_created[MAX_COUNT];
static int device_created[MAX_COUNT];


static size_t size = 1024;
int param_size_set(const char *val, const struct kernel_param *kp)
{
    int nsize, res;

    if (kstrtouint(val, 0, &nsize))
    {
        pr_err("BUFFER: bad size format\n");
        return -ERANGE;
    }
    nsize = size;
    res = param_set_uint(val, kp);
    if (res == 0)
    {
        pr_info("BUFFER: new size = %lu, old size = %d\n", size, nsize);
        for (int i = 0; i < MAX_COUNT; ++i) {
            if (device_usage[i] == 0) {
                buffer_size[i] = size;
            }
        }
        return 0;
    }

    pr_err("BUFFER: unknown error\n");
    return -ERANGE;
}
const struct kernel_param_ops param_size_of =
{
        .set = &param_size_set,
        .get = &param_get_ulong,
};

module_param_cb(size, &param_size_of, &size, 0644);
MODULE_PARM_DESC(size, "Default size of buffer");

dev_t dev = 0;
static struct cdev chrdev_cdev[MAX_COUNT];
static struct class *chrdev_class;
//TODO add something to change size of already existing buffers

static size_t count = 0;

dev_t set_minor(dev_t dev, int minor) {
    return MKDEV(MAJOR(dev), minor);
}

void unload_all_device(void) {
    int i;
    dev_t cur_dev = set_minor(dev, 0);
    for (i = 0; i < MAX_COUNT; ++i) {
        if (device_created[i]) {
            device_destroy(chrdev_class, cur_dev);
        }
        ++cur_dev;
    }
}

void unload_all_cdev(void) {
    for (int i = 0; i < MAX_COUNT; ++i) {
        if (chrdev_created[i]) {
            cdev_del(chrdev_cdev + i);
        }
    }
}

void free_all_buffers(void) {
    for (int i = 0; i < MAX_COUNT; ++i) {
        if (device_usage[i] > 0) {
            kfree(buffer + i);
        }
    }
}

int param_count_set(const char *val, const struct kernel_param *kp)
{
    int ncount, res;
    char str[64];
    dev_t cur_dev;

    if (kstrtoint(val, 0, &ncount))
    {
        pr_err("BUFFER: bad count format\n");
        return -ERANGE;
    }

    if (ncount >= MAX_COUNT)
    {
        pr_err("BUFFER: max count exceeded, should be <%d, given %d\n", MAX_COUNT, ncount);
        return -ERANGE;
    }
    ncount = count;
    res = param_set_uint(val, kp);
    if (res == 0)
    {
        pr_info("BUFFER: new count = %lu, old count = %d\n", count, ncount);
        if (ncount < count) {
            //Old was less than new, need to create new devices
            while (ncount < count) {
                down_write(buffer_lock + ncount); //lock
                dev = set_minor(dev, ncount);
                if ((res = cdev_add(chrdev_cdev + ncount, dev, 1)) < 0) {
                    pr_err("CHRDEV: device registering error\n");
                    unload_all_device();
                    class_destroy(chrdev_class);
                    unload_all_cdev();
                    unregister_chrdev_region(dev, 1);
                    return -res;
                }
                buffer_size[ncount] = size;
                device_usage[ncount] = 0;
                chrdev_created[ncount] = TRUE;
                //buffer[ncount] = kzalloc(buffer_size[ncount], GFP_KERNEL); //Will be allocated when open device

                cur_dev = set_minor(dev, ncount);
                sprintf(str, "buffer!buffer_device_%d", ncount);
                if (IS_ERR(device_create(chrdev_class, NULL, cur_dev, NULL, str))) {
                    pr_err("CHRDEV: error creating device\n");
                    unload_all_device();
                    class_destroy(chrdev_class);
                    unload_all_cdev();
                    unregister_chrdev_region(dev, 1);
                    return -1;
                }
                device_created[ncount] = TRUE;
                up_write(buffer_lock + ncount);  //unlock
                pr_info("BUFFER: allocated %lu bytes for %d device\n", size, ncount);
                ++ncount;
            }
        }
        else if (ncount > count) {
            //Old was bigger than new, need to delete //TODO chdev del?
            //Lock everything for write and check if all deleting buffers is not usable right now
            int index;
            for (index = count; index < ncount; ++index) {
                down_write(buffer_lock + index);
            }
            int isSomethingOpen = FALSE;
            for (index = count; index < ncount; ++index) {
                if (device_usage[index] != 0) {
                    isSomethingOpen = TRUE;
                }
            }
            if (isSomethingOpen) {
                pr_info("BUFFER: Can't change size from %d to %d because some devices is open, rollback", ncount, count);
                count = ncount; //Return old value
            } else {
                index = ncount;
                while (index > count) {
                    --index;
                    kfree(buffer[index]);
                    buffer[index] = 0;
                    chrdev_created[index] = FALSE;
                    cdev_del(chrdev_cdev + index);
                    pr_info("BUFFER: free %d device\n", index);
                }
            }
            for (index = count; index < ncount; ++index) {
                up_write(buffer_lock + index);
            }
        } else {
            pr_info("BUFFER: count don't change\n");
        }
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

static size_t device_size = 1024;
module_param(device_size, ulong, 0644);
MODULE_PARM_DESC(device_size, "Set device size before device_minor");

static int device_minor = -1;

int device_minor_set(const char *val, const struct kernel_param *kp)
{
    int minor, res;

    if (kstrtoint(val, 0, &minor))
    {
        pr_err("BUFFER: bad device_minor format\n");
        return -ERANGE;
    }

    if (minor >= MAX_COUNT)
    {
        pr_err("BUFFER: max device_minor exceeded, should be <%d, given %d\n", MAX_COUNT, minor);
        return -ERANGE;
    }
    res = param_set_int(val, kp);
    if (res == 0)
    {
        pr_info("BUFFER: new device_minor = %d, current device_size = %lu\n", device_minor, device_size);
        if (device_minor >= 0) {
            if (chrdev_created[minor]) {
                down_write(buffer_lock + device_minor);
                buffer_size[device_minor] = device_size;
                if (device_usage[minor] > 0) {
                    kfree(buffer + device_minor);
                    buffer[device_minor] = kzalloc(buffer_size[device_minor], GFP_KERNEL); //FIXME ???
                }
                up_write(buffer_lock + device_minor);
            } else {
                pr_info("BUFFER: device not yet created");
            }
        }
        return 0;
    }

    pr_err("BUFFER: unknown error\n");
    return -ERANGE;
}
const struct kernel_param_ops device_minor_of =
{
        .set = &device_minor_set,
        .get = &param_get_int,
};

module_param_cb(device_minor, &device_minor_of, &device_minor, 0644);
MODULE_PARM_DESC(device_minor, "Set minor device number after setting device_size");


static ssize_t chrdev_read(struct file *file, char __user *buf, size_t length, loff_t *off)
{
    size_t offset = *off;
    ssize_t uncopy;
    unsigned int minor = (uintptr_t) file->private_data;
    pr_info("BUFFER: device %u, reader with offset %lld and length %ld\n", minor, *off, length);

    down_read(buffer_lock + minor);
    if (length + offset > buffer_size[minor])
        length = buffer_size[minor] - offset;
    uncopy = copy_to_user(buf, buffer[minor] + offset, length);
    up_read(buffer_lock + minor);
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
        pr_err("BUFFER: can't alloc memory for tmp buffer\n");
        return -ENOMEM;
    }

    if (length + offset > buffer_size[minor])
        length = buffer_size[minor] - offset;
    uncopy = copy_from_user(tmp_buffer, buf, length);

    if (uncopy > 0) {
        pr_err("BUFFER: copy_from_user failed, doesn't copy %lu bytes\n", uncopy);
        kfree(tmp_buffer);
        return -EFAULT;
    }
    down_write(buffer_lock + minor);
    memcpy(buffer[minor] + offset, tmp_buffer, length);
    up_write(buffer_lock + minor);
    *off += length;
    return length;
}

static int chrdev_open(struct inode *inode, struct file *file) {
    int minor = iminor(inode);

    if (device_usage[minor] == 0) {
        buffer[minor] = kzalloc(buffer_size[minor], GFP_KERNEL);
        if (buffer[minor] == NULL) {    
            pr_err("BUFFER: can't init %d device buffer\n", minor);
            return -1;
        }
    }
    ++device_usage[minor];
    pr_info("BUFFER: open %d device, current usage: %lu\n", minor, device_usage[minor]);
    file->private_data = (void *) (uintptr_t) minor;
    return 0;
}

static int chrdev_release(struct inode *inode, struct file *file) {
    int minor = iminor(inode);
    --device_usage[minor];
    pr_info("BUFFER: close %d device, current usage: %lu\n", minor, device_usage[minor]);
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

static int __init module_start(void)
{
    int i, res;
    dev_t cur_dev;
    char str[64];

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
    pr_info("CHRDEV load: Major = %d\n", MAJOR(dev));
    for (i = 0; i < MAX_COUNT; ++i) {
        device_usage[i] = 0;
        buffer[i] = 0;
        init_rwsem(buffer_lock + i);
        cdev_init(chrdev_cdev + i, &chrdev_ops);
    }
    for (i = 0; i < count; ++i) {
        dev = set_minor(dev, i);
        if ((res = cdev_add(chrdev_cdev + i, dev, 1)) < 0) {
            pr_err("CHRDEV: device registering error for number %d\n", i);
            unload_all_cdev();
            unregister_chrdev_region(dev, 1);
            return res;
        }
        chrdev_created[i] = TRUE;
    }

    if (IS_ERR(chrdev_class = class_create(THIS_MODULE, "buffer_class"))) {
        unload_all_cdev();
        unregister_chrdev_region(dev, 1); //FIXME
        return -1;
    }

    cur_dev = set_minor(dev, 0);
    for (int i = 0; i < count; ++i, ++cur_dev) { //FIXME change to count and add to
        sprintf(str, "buffer!buffer_device_%d", i);
        if (IS_ERR(device_create(chrdev_class, NULL, cur_dev, NULL, str))) {
            pr_err("CHRDEV: error creating device\n");
            unload_all_device();
            class_destroy(chrdev_class);
            unload_all_cdev();
            unregister_chrdev_region(dev, 1);
            return -1;
        }
        device_created[i] = TRUE;
    }
    return 0;
}

static void __exit module_end(void)
{
    unload_all_device();
    class_destroy (chrdev_class);
    free_all_buffers();
    unload_all_cdev();
    unregister_chrdev_region(dev, 1);
    pr_info("BUFFER: unload\n");
}

module_init(module_start);
module_exit(module_end);