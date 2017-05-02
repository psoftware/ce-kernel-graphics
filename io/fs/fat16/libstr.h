//fonte http://stackoverflow.com/questions/2488563/strcat-implementation
char * strcat(char *dest, const char *src)
{
    size_t i,j;
    for (i = 0; dest[i] != '\0'; i++);
    for (j = 0; src[j] != '\0'; j++)
        dest[i+j] = src[j];
    dest[i+j] = '\0';
    return dest;
}

bool streq(const char *str1, const char *str2)
{
	int str1_len = strlen(str1);
	int str2_len = strlen(str2);

	if(str1_len!=str2_len)
		return false;

	for(int i=0; i<str1_len; i++)
		if(str1[i]!=str2[i])
			return false;

	return true;
}

void substring(char *result, const char *source, int first_index, int latest_index)
{
	for(int i=first_index; i<=latest_index; i++)
		result[i-first_index] = source[i];

	result[latest_index+1] = '\0';
}

bool extract_sublevel_filename(char *result, const char *source, natb level)
{
	natb previous_slash = 0;
	natb temp_level = 0;
	for(natb i=1; i<strlen(source)+1; i++)
		if(source[i]=='/' || source[i]=='\0')
		{
			if(i==previous_slash+1)	//condizione per gestire i doppi slash
			{
				previous_slash++;
				continue;
			}

			if(temp_level==level) //
			{
				substring(result, source, previous_slash+1, i-1);
				return true;
			}

			temp_level++;
			previous_slash=i;
		}

	return false;
}