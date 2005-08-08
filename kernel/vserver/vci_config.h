
enum {
	VCI_KCBIT_LEGACY = 1,
	VCI_KCBIT_LEGACYNET,
	VCI_KCBIT_NGNET,

	VCI_KCBIT_PROC_SECURE,
	VCI_KCBIT_HARDCPU,
	VCI_KCBIT_HARDCPU_IDLE,

	VCI_KCBIT_DEBUG = 16,
	VCI_KCBIT_HISTORY = 20,
	VCI_KCBIT_TAGXID = 24,
};


static inline uint32_t vci_kernel_config(void)
{
	return
	/* various legacy options */
#ifdef	CONFIG_VSERVER_LEGACY
	(1 << VCI_KCBIT_LEGACY) |
#endif
#ifdef	CONFIG_VSERVER_LEGACYNET
	(1 << VCI_KCBIT_LEGACYNET) |
#endif

	/* configured features */
#ifdef	CONFIG_VSERVER_PROC_SECURE
	(1 << VCI_KCBIT_PROC_SECURE) |
#endif
#ifdef	CONFIG_VSERVER_HARDCPU
	(1 << VCI_KCBIT_HARDCPU) |
#endif
#ifdef	CONFIG_VSERVER_HARDCPU_IDLE
	(1 << VCI_KCBIT_HARDCPU_IDLE) |
#endif

	/* debug options */
#ifdef	CONFIG_VSERVER_DEBUG
	(1 << VCI_KCBIT_DEBUG) |
#endif
#ifdef	CONFIG_VSERVER_HISTORY
	(1 << VCI_KCBIT_HISTORY) |
#endif

	/* inode xid tagging */
#if	defined(CONFIG_INOXID_NONE)
	(0 << VCI_KCBIT_TAGXID) |
#elif	defined(CONFIG_INOXID_UID16)
	(1 << VCI_KCBIT_TAGXID) |
#elif	defined(CONFIG_INOXID_GID16)
	(2 << VCI_KCBIT_TAGXID) |
#elif	defined(CONFIG_INOXID_UGID24)
	(3 << VCI_KCBIT_TAGXID) |
#elif	defined(CONFIG_INOXID_INTERN)
	(4 << VCI_KCBIT_TAGXID) |
#elif	defined(CONFIG_INOXID_RUNTIME)
	(5 << VCI_KCBIT_TAGXID) |
#else
	(7 << VCI_KCBIT_TAGXID) |
#endif
	0;
}

