#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched/mm.h>

int RSS = 0;
int WSS = 0;
int SWAP = 0;

pgd_t *pgd;
p4d_t *p4d;
pmd_t *pmd;
pud_t *pud;
pte_t *ptep, pte;

struct task_struct *task;

static int pid = 0;
module_param(pid, int, 0000);

void page_table_walk(struct mm_struct *mm, unsigned long address) {
	pgd = pgd_offset(mm,address);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		return;
	}
	p4d = p4d_offset(pgd, address);
	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
		return;
	}
	pud = pud_offset(p4d, address);
	if (pud_none(*pud) || pud_bad(*pud)) {
		return;
	}
	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		return;
	}
	ptep = pte_offset_map(pmd, address);
	if (!ptep) {
		return;
	}
	pte = *ptep;
}

int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {
	int ret = 0;
	if (pte_young(*ptep)) {
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) &ptep->pte);
	}
	return ret;
}

unsigned long timer_interval_ns = 10e9;
static struct hrtimer hr_timer;
enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart) {
	struct vm_area_struct *vma;
	struct mm_struct *mem;
	WSS = 0;
	RSS = 1;
	SWAP = 0;
	ktime_t currtime, interval;	
	
	currtime = ktime_get();
	interval = ktime_set(0,timer_interval_ns);
	hrtimer_forward(timer_for_restart, currtime, interval);
	
	if (task->mm != NULL) {
		vma = task->mm->mmap;
		mem = task->mm;
		while (vma != NULL) {
			unsigned long address = vma->vm_start;
			
			while(address != vma->vm_end) {
				page_table_walk(mem, address);
				if (ptep) {
					if (pte_present(*ptep) == 1) {
						RSS++;
						if (ptep_test_and_clear_young(vma, address, ptep) == 1) {
							WSS++;
							test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *)ptep);
						}
					}else {
					
						SWAP++;
					};
				}
				address += PAGE_SIZE;
				ptep = NULL;	
			}
			
			page_table_walk(mem, address);
			if (ptep) {
				if (pte_present(*ptep) == 1) {
					RSS++;
					if (ptep_test_and_clear_young(vma, address, ptep) == 1) {
						WSS++;
						test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *)ptep);
					}
				}else {
				
					SWAP++;
				};
			}
			ptep = NULL;
			
			vma = vma->vm_next;
		}
	}
	printk("PID %d: RSS=%d KB, SWAP=%d KB, WSS=%d KB\n", pid, 4*RSS, 4*SWAP, 4*WSS);
	return HRTIMER_RESTART;
}

static int __init timer_init(void) {
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	ktime_t ktime;
	ktime = ktime_set(0, timer_interval_ns);
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &timer_callback;
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
	return 0;
}

static void __exit timer_exit(void) {
	int ret;
	ret = hrtimer_cancel(&hr_timer);
	if (ret) printk("The timer was still in use...\n");
	printk("HR Timer module uninstalling\n");
}

module_init(timer_init);
module_exit(timer_exit);


MODULE_LICENSE("GPL");
