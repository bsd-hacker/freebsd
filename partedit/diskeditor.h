struct partedit_item {
	int indentation;
	const char *name;
	size_t size;
	const char *type;
	char *mountpoint;

	void *cookie;
};

int diskeditor_show(const char *title, const char *prompt,
    struct partedit_item *items, int nitems, int *selected, int *scroll);

