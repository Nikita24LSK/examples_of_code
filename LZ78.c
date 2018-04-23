#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

void compress(char **, char *, char *, FILE *, FILE *);
long find_word(char **, char *, char *, char);
char cmp_words(char **, char, char *, char);
void add_to_dict(char **, char *, char *, char);
void decompress(char **, char *, char *, FILE *, FILE *);

int main(int argc, char *argv[])
{

	FILE *data, *proc_data;
	char data_size, data_half_size, *len_words;
	char **dict, *buff;
	int i;

	if(argc < 4)
	{
		printf("\nUsage: LZ78 [cd] <input file> <output file>\n");
		exit(0);
	}

	if((data = fopen(argv[2], "rb")) == NULL)
	{
		printf("\nError: Input file is not exist!\n");
		exit(0);
	}

	fseek(data, 0, SEEK_END);
	data_size = ftell(data);

	if(data_size < 2)
	{
		printf("\nError: The file is too small!\n");
		fclose(data);
		exit(0);
	}

	data_half_size = data_size >> 1;
	fseek(data, 0, SEEK_SET);
	if((proc_data = fopen(argv[3], "wb")) == NULL)
	{
		printf("\nError: Cannot open output file!\n");
		fclose(data);
		exit(0);
	}

	dict = (char**)malloc(data_size * sizeof(char*));
	len_words = (char*)calloc(data_size, sizeof(char));
	buff = (char*)calloc(data_half_size, sizeof(char));
	for (i = 0; i < data_size; i++)
	{
		dict[i] = (char*)malloc(data_half_size * sizeof(char));
	}

	if(!strcmp(argv[1], "c"))
	{
		compress(dict, len_words, buff, data, proc_data);
	}
	else if(!strcmp(argv[1], "d"))
	{
		decompress(dict, len_words, buff, data, proc_data);
	}
	else
	{
		printf("\nError: Unknown argument!\n");
	}

	fclose(data);
	fclose(proc_data);
	for(i = 0; i < data_size; i++)
	{
		free(dict[i]);
	}
	free(dict);
	free(buff);
	free(len_words);
	return 0;
}

void compress(char **dict, char *len_words, char *buff, FILE *data, FILE *proc_data)
{
	char ch;
	char len_buff = 0;
	char word_index = -1, temp;

	while (fread(&ch, sizeof(char), 1, data))
	{
		buff[len_buff] = ch;
		len_buff++;
		if((temp = find_word(dict, len_words, buff, len_buff)) != -1)
		{
			word_index = temp;
		}
		else
		{
			fprintf(proc_data, "%c", word_index+1);
			fprintf(proc_data, "%c", ch);
			add_to_dict(dict, len_words, buff, len_buff);
			len_buff = 0;
			word_index = -1;
		}
	}
}

long find_word(char **dict, char *len_words, char *buff, char len_buff)
{
	char i = 0, index = -1;

	while(len_words[i] != 0)
	{
		if(len_words[i] == len_buff)
		{
			if(cmp_words(dict, i, buff, len_buff))
			{
				index = i;
			}
		}
		i++;
	}

	return index;
}

char cmp_words(char **dict, char str, char *buff, char len_buff)
{
	int i;

	for(i = 0; i < len_buff; i++)
	{
		if(buff[i] != dict[str][i])
		{
			return 0;
		}
	}

	return 1;
}

void add_to_dict(char **dict, char *len_words, char *buff, char len_buff)
{
	char i = 0, j;

	while(len_words[i] != 0)
	{
		i++;
	}

	len_words[i] = len_buff;

	for(j = 0; j < len_buff; j++)
	{
		dict[i][j] = buff[j];
	}
}

void decompress(char **dict, char *len_words, char *buff, FILE *data, FILE *proc_data)
{
	char ch, flag = 0, i, index = -1;

	while(fread(&ch, sizeof(char), 1, data))
	{
		if(!flag)
		{
			index = ch - 1;
			flag = 1;
		}
		else
		{
			if(index == -1)
			{
				fprintf(proc_data, "%c", ch);
				add_to_dict(dict, len_words, &ch, 1);
			}
			else
			{
				for(i = 0; i < len_words[index]; i++)
				{
					buff[i] = dict[index][i];
					fprintf(proc_data, "%c", buff[i]);
				}
				buff[i] = ch;
				fprintf(proc_data, "%c", ch);
				add_to_dict(dict, len_words, buff, i+1);
			}
			flag = 0;
		}
	}
}