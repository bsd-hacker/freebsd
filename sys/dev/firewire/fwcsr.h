void fwcsr_busy_timeout_init(struct fw_bind *, struct firewire_dev_comm *,
                          void *, void *, struct malloc_type *, uint32_t,
                          uint32_t, uint32_t);
void fwcsr_busy_timeout_stop(struct fw_bind *, struct firewire_dev_comm *);

void fwcsr_reset_start_init(struct fw_bind *, struct firewire_dev_comm *,
                          void *, void *, struct malloc_type *, uint32_t,
                          uint32_t, uint32_t);
void fwcsr_reset_start_stop(struct fw_bind *, struct firewire_dev_comm *);
