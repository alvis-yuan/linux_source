

struct pattern_filter {
	struct filter_node node;
	regex_t regex;
};

static struct pattern_filter *pattern_filters;


static void filter_add_pattern(const char *pattern)
 {
	 struct pattern_filter *filter = malloc(sizeof(*filter));
	 if (!filter)
		 return;
 
	 if (regcomp(&filter->regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
		 free(filter);
		 return;
	 }
 
	 filter->next = pattern_filters;
	 pattern_filters = filter;
 }
 
 static void filter_remove_pattern(const char *pattern)
 {
	 struct pattern_filter **p = &pattern_filters;
	 while (*p) {
		 if (regexec(&(*p)->regex, pattern, 0, NULL, 0) == 0) {
			 struct pattern_filter *tmp = *p;
			 *p = (*p)->next;
			 regfree(&tmp->regex);
			 free(tmp);
			 return;
		 }
		 p = &(*p)->next;
	 }
 }

 