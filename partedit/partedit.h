#include <sys/queue.h>
#include <fstab.h>

struct gprovider;
struct gmesh;
struct ggeom;

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

int part_wizard(void);

/* gpart operations */
void gpart_delete(struct gprovider *pp);
void gpart_destroy(struct ggeom *lg_geom, int force);
void gpart_edit(struct gprovider *pp);
void gpart_create(struct gprovider *pp, char *default_type, char *default_size,
    char *default_mountpoint, char **output, int interactive);
void gpart_revert(struct gprovider *pp);
void gpart_revert_all(struct gmesh *mesh);
void gpart_commit(struct gmesh *mesh);
int gpart_partition(const char *lg_name, const char *scheme);
void set_default_part_metadata(const char *name, const char *scheme,
    const char *type, const char *mountpoint, int newfs);

/* machine-dependent bootability checks */
const char *default_scheme(void);
int is_scheme_bootable(const char *part_type);
size_t bootpart_size(const char *part_type);
const char *bootcode_path(const char *part_type);
const char *partcode_path(const char *part_type);
