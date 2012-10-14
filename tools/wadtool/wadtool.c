/**
 * @file wadtool.c
 * WAD creation tool. This is ANSI C but unfortunately only compiles on
 * Windows (uses Win32 file finding).
 *
 * @author Copyright © 2005-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <io.h> 
#include "wadtool.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fname_t root;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// NewFile
//===========================================================================
void NewFile(const char *path, int size)
{
	fname_t *fn = malloc(sizeof(fname_t));

	memset(fn, 0, sizeof(*fn));
	strncpy(fn->path, path, 255);
	// Link it in just previous of root.
	fn->next = &root;
	fn->prev = root.prev;
	fn->prev->next = fn;
	root.prev = fn;
	fn->size = size;
}

//===========================================================================
// InitList
//===========================================================================
void InitList(void)
{
	root.next = root.prev = &root;
}

//===========================================================================
// DestroyList
//===========================================================================
void DestroyList(void)
{
	fname_t *it = root.next, *next;
	for(; it != &root; it = next)
	{
		next = it->next;
		free(it);
	}
	InitList();
}

//===========================================================================
// CollectFiles
//===========================================================================
void CollectFiles(const char *basepath)
{
	long hFile;
    struct _finddata_t fd;
	char findspec[256], path[256];

	sprintf(findspec, "%s*.*", basepath);
	if((hFile = _findfirst(findspec, &fd)) != -1L)
	{
		// The first file found!
		do 
		{
			if(!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
				continue;
			sprintf(path, "%s%s", basepath, fd.name);
			if(fd.attrib & _A_SUBDIR)
			{
				strcat(path, "\\");
				CollectFiles(path);
			}
			else
			{
				NewFile(path, fd.size);
			}
		} 
		while(!_findnext(hFile, &fd));
	}
}

//===========================================================================
// CountList
//===========================================================================
int CountList(void)
{
	int count = 0;
	fname_t *it;

	for(it = root.next; it != &root; it = it->next) count++;
	return count;
}

//===========================================================================
// CopyToStream
//	Returns true if succesful.
//===========================================================================
int CopyToStream(FILE *file, fname_t *fn)
{
	FILE *in = fopen(fn->path, "rb");
	char *buf;

	if(!in) return 0;
	buf = malloc(fn->size);
	fread(buf, fn->size, 1, in);
	fwrite(buf, fn->size, 1, file);
	free(buf);
	fclose(in);
	return 1;
}

//===========================================================================
// PrintBanner
//===========================================================================
void PrintBanner(void)
{
	printf("### The WAD Tool v"VERSION_STR" by Jaakko Ker\x084nen ###\n");
}

//===========================================================================
// PrintUsage
//===========================================================================
void PrintUsage(void)
{
	printf("Usage: wadtool newfile.wad [dir-prefix]\n");
}

//===========================================================================
// main
//===========================================================================
int main(int argc, char **argv)
{
	char *wadfile;
	char *prefix = "";
	FILE *file;
	wadinfo_t hdr;
	lumpinfo_t info;
	fname_t *it;
	int c;
	char lumpbase[4];
	int direc_size, direc_offset;

	PrintBanner();
	if(argc < 2 || argc > 3)
	{
		PrintUsage();
		return 0;
	}
	wadfile = argv[1];
	if(argc > 2) prefix = argv[2];

    srand((unsigned int)time(0));
	rand();
	rand();

	// First compile the list of all file names.
	InitList();
	printf("Collecting file names...\n");
	CollectFiles("");	
	printf("Creating WAD file %s...\n", wadfile);
	if((file = fopen(wadfile, "wb")) == NULL)
	{
		printf("Couldn't open %s.\n", wadfile);
		perror("Error");
		goto stop_now;
	}

	// The header.
	hdr.identification[0] = 'P';
	hdr.identification[1] = 'W';
	hdr.identification[2] = 'A';
	hdr.identification[3] = 'D';
	hdr.numlumps = CountList() + 1;
	hdr.infotableofs = 0; // We've no idea yet.
	fwrite(&hdr, sizeof(hdr), 1, file);

	// Write all the files.
	sprintf(lumpbase, "%c%c", 'A' + rand() % 26, 'A' + rand() % 26);
	for(it = root.next, c = 0; it != &root; it = it->next, c++)
	{
		it->offset = ftell(file);
		if(!CopyToStream(file, it)) 
		{
			perror(it->path);
			goto stop_now;
		}
		printf("%s\n", it->path);
		sprintf(it->lump, "__%s%04X", lumpbase, c);			
	}

	// Write DD_DIREC. 
	direc_offset = ftell(file);
	for(it = root.next; it != &root; it = it->next)
		fprintf(file, "%s %s%s\n", it->lump, prefix, it->path);
	direc_size = ftell(file) - direc_offset;
	
	// Time to write the info table.
	hdr.infotableofs = ftell(file);
	for(it = root.next, c = 0; it != &root; it = it->next, c++)
	{
		memset(&info, 0, sizeof(info));
		info.filepos = it->offset;
		info.size = it->size;
		memcpy(info.name, it->lump, 8);
		fwrite(&info, sizeof(info), 1, file);
	}
	// Finally DD_DIREC's entry.
	info.filepos = direc_offset;
	info.size = direc_size;
	strncpy(info.name, "DD_DIREC", 8);
	fwrite(&info, sizeof(info), 1, file);

	// Rewrite the header.
	rewind(file);
	fwrite(&hdr, sizeof(hdr), 1, file);
	
	// We're done!
	fclose(file);
stop_now:
	DestroyList();
	return 0;
}
