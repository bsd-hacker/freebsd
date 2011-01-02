#include <sys/queue.h>
#include <fstab.h>

struct gprovider;
struct gmesh;

TAILQ_HEAD(pmetadata_head, partition_metadata);
extern struct pmetadata_head part_metadata;

struct partition_metadata {
	char *name;		/* name of this partition, as in GEOM */
	
	struct fstab *fstab;	/* fstab data for this partition */
	char *newfs;		/* shell command to initialize partition */
	
	int bootcode;

	TAILQ_ENTRY(partition_metadata) metadata;
};

struct partition_metadata *get_part_metadata(const char *name, int create);
void delete_part_metadata(const char *name);

/* gpart operations */
void gpart_delete(struct gprovider *pp);
void gpart_edit(struct gprovider *pp);
void gpart_create(struct gprovider *pp);
void gpart_revert(struct gprovider *pp);
void gpart_revert_all(struct gmesh *mesh);
void gpart_commit(struct gmesh *mesh);

/* machine-dependent bootability checks */
int is_scheme_bootable(const char *part_type);
size_t bootpart_size(const char *part_type);
const char *bootcode_path(const char *part_type);
const char *partcode_path(const char *part_type);
