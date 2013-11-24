/*-
 * Copyright (c) 2013 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elfexplorer.h"
#include "util.h"

/*
 * Compare two sections by their names.
 */
static int
elfx_compare_sections_by_name(const void *ap, const void *bp)
{
	const elfx_section *a = ap, *b = bp;

	return (strcmp(a->name, b->name));
}

/*
 * Compare two sections by their start addresses.
 */
static int
elfx_compare_sections_by_addr(const void *ap, const void *bp)
{
	const elfx_section *a = ap, *b = bp;

	/* no overlap allowed */
	assert(a == b || a->baddr == 0 ||
	    a->eaddr < b->baddr || a->baddr > b->eaddr);
	return (a->baddr < b->baddr ? -1 : a->baddr > b->baddr ? 1 : 0);
}

/*
 * Compare two symbols by their names.
 */
static int
elfx_compare_symbols_by_name(const void *ap, const void *bp)
{
	const elfx_symbol *a = ap, *b = bp;

	return (strcmp(a->name, b->name));
}

/*
 * Free memory that was allocated to store information about sections.
 */
static void
elfx_free_sections(elfx_file *ef)
{

	if (ef == NULL)
		return;
	if (ef->sections != NULL) {
		for (unsigned int i = 0; i < ef->nsections; ++i)
			if (ef->sections[i].ptr && ef->sections[i].data &&
			    ef->sections[i].ptr != ef->sections[i].data->d_buf)
				free(ef->sections[i].ptr);
		free(ef->sections);
		ef->sections = NULL;
	}
	if (ef->sections_by_name != NULL) {
		free(ef->sections_by_name);
		ef->sections_by_name = NULL;
	}
	if (ef->sections_by_addr != NULL) {
		free(ef->sections_by_addr);
		ef->sections_by_addr = NULL;
	}
	ef->nsections = 0;
}

/*
 * Retrieve information about sections.
 */
static int
elfx_load_sections(elfx_file *ef)
{
	elfx_section *es;
	size_t size;

	/* get translated ELF header */
	if (gelf_getehdr(ef->elf, &ef->hdr) == NULL)
		goto elf_error;
	/* get number of sections in file */
	if (elf_getshdrnum(ef->elf, &size) != ELF_E_NONE)
		goto elf_error;
	ef->nsections = size;
	size *= sizeof *ef->sections;
	if ((ef->sections = calloc(size, 1)) == NULL)
		return (-1);
	verbose("%4s %4s %8s %8s %s", "sect", "type", "start", "size", "name");
	/* iterate over sections */
	for (unsigned int i = 0; i < ef->nsections; ++i) {
		es = &ef->sections[i];
		es->file = ef;
		es->index = i;
		/* get section from file */
		if ((es->scn = elf_getscn(ef->elf, i)) == NULL)
			goto elf_error;
		/* get translated section header */
		if ((gelf_getshdr(es->scn, &es->hdr)) == NULL)
			goto elf_error;
		/* get section name */
		if ((es->name = elf_strptr(ef->elf, ef->hdr.e_shstrndx,
		    es->hdr.sh_name)) == NULL)
			goto elf_error;
		verbose("%4d %4d %08x %08lx %s", elf_ndxscn(es->scn),
		    es->hdr.sh_type, (unsigned long)es->hdr.sh_addr,
		    (unsigned long)es->hdr.sh_size, es->name);
		/* get data, if any */
		if ((es->data = elf_getdata(es->scn, NULL)) != NULL) {
			assert(es->data->d_off == 0 &&
			    es->data->d_size == es->hdr.sh_size);
			if (es->hdr.sh_type == SHT_NOBITS)
				es->ptr = calloc(es->hdr.sh_size, 1);
			else
				es->ptr = es->data->d_buf;
			es->baddr = es->hdr.sh_addr;
			es->eaddr = es->hdr.sh_addr + es->hdr.sh_size - 1;
			es->size = es->hdr.sh_size;
			/* only one data descriptor */
			assert(elf_getdata(es->scn, es->data) == NULL);
		}
	}
	/* create copy of section list, sorted by name */
	if ((ef->sections_by_name = calloc(size, 1)) == NULL)
		return (-1);
	memcpy(ef->sections_by_name, ef->sections, size);
	mergesort(ef->sections_by_name, ef->nsections,
	    sizeof *ef->sections_by_name, elfx_compare_sections_by_name);
	/* create copy of section list, sorted by address */
	if ((ef->sections_by_addr = calloc(size, 1)) == NULL)
		return (-1);
	memcpy(ef->sections_by_addr, ef->sections, size);
	mergesort(ef->sections_by_addr, ef->nsections,
	    sizeof *ef->sections_by_addr, elfx_compare_sections_by_addr);
	return (ef->nsections);
elf_error:
	info("%s: %s", ef->path, elf_errmsg(elf_errno()));
	return (-1);
}

/*
 * Retrieve and sort the symbol table.
 */
static int
elfx_load_symbols(elfx_file *ef)
{
	elfx_section *symtab;
	GElf_Sym sym;
	size_t size;

	if ((symtab = elfx_get_section_by_name(ef, ".symtab")) == NULL)
		return (-1);
	ef->nsymbols = size = symtab->hdr.sh_size / symtab->hdr.sh_entsize;
	size *= sizeof *ef->symbols;
	if ((ef->symbols = calloc(size, 1)) == NULL)
		return (-1);
	for (unsigned int i = 0; i < ef->nsymbols; ++i) {
		gelf_getsym(symtab->data, i, &sym);
		ef->symbols[i].file = ef;
		ef->symbols[i].name = elf_strptr(ef->elf, symtab->hdr.sh_link,
		    sym.st_name);
		ef->symbols[i].addr = sym.st_value;
		ef->symbols[i].size = sym.st_size;
	}
	mergesort(ef->symbols, ef->nsymbols, sizeof *ef->symbols,
	    elfx_compare_symbols_by_name);
	return (0);
}

/*
 * Open an ELF file and retrieve the information we need.
 */
elfx_file *
elfx_open(const char *path)
{
	elfx_file *ef;
	struct stat sb;

	if ((ef = calloc(sizeof *ef, 1)) == NULL)
		goto fail;
	if (realpath(path, ef->path) == NULL)
		goto fail;
	if ((ef->fd = open(ef->path, O_RDONLY)) == -1)
		goto fail;
	if (fstat(ef->fd, &sb) != 0)
		goto fail;
	ef->size = sb.st_size;
	if ((ef->map = mmap(NULL, ef->size, PROT_READ, MAP_PRIVATE,
	    ef->fd, 0)) == NULL)
		goto fail;
	if ((ef->elf = elf_memory(ef->map, ef->size)) == NULL)
		goto fail;
	if (elf_kind(ef->elf) != ELF_K_ELF)
		goto fail;
	if (elfx_load_sections(ef) < 0)
		goto fail;
	if (elfx_load_symbols(ef) < 0)
		goto fail;
	return (ef);
fail:
	elfx_close(ef);
	return (NULL);
}

/*
 * Close an ELF file and free all allocated memory.
 */
void
elfx_close(elfx_file *ef)
{

	if (ef == NULL)
		return;
	elfx_free_sections(ef);
	if (ef->elf != NULL)
		elf_end(ef->elf);
	if (ef->map != NULL)
		munmap(ef->map, ef->size);
	if (ef->fd <= 0)
		close(ef->fd);
	free(ef);
}

/*
 * Retrieve a section by its name.
 */
elfx_section *
elfx_get_section_by_name(elfx_file *ef, const char *name)
{
	elfx_section *es;
	int lo, hi, mid;
	int cmp;

	if (ef->last_section_by_name != 0 &&
	    strcmp(name, ef->last_section_by_name->name) == 0)
		return (ef->last_section_by_name);
	es = ef->sections_by_name;
	lo = 0;
	hi = ef->nsections - 1;
	for (;;) {
		mid = (lo + hi) / 2;
//		verbose("(%d, %d, %d) %s == %s",
//		    lo, hi, mid, es[mid].name, name);
		if ((cmp = strcmp(name, es[mid].name)) == 0)
			return (ef->last_section_by_name = &es[mid]);
		else if (lo == hi)
			return (NULL);
		else if (cmp < 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
}

/*
 * Retrieve the section that contains a specified address.
 */
elfx_section *
elfx_get_section_by_addr(elfx_file *ef, uintptr_t addr)
{
	elfx_section *es;
	int lo, hi, mid;

	if (ef->last_section_by_addr != NULL &&
	    ef->last_section_by_addr->baddr <= addr &&
	    addr <= ef->last_section_by_addr->eaddr)
		return (ef->last_section_by_addr);
	es = ef->sections_by_addr;
	lo = 0;
	hi = ef->nsections - 1;
	for (;;) {
		mid = (lo + hi) / 2;
//		verbose("(%d, %d, %d) %08x <= %08x <= %08x", lo, mid, hi,
//		    es[mid].baddr, addr, es[mid].eaddr);
		if (es[mid].baddr <= addr && addr <= es[mid].eaddr)
			return (ef->last_section_by_addr = &es[mid]);
		else if (lo == hi)
			return (NULL);
		else if (addr < es[mid].baddr)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
}

/*
 * Return a pointer to the data at the specified address.
 */
void *
elfx_get_data(elfx_file *ef, uintptr_t addr)
{
	elfx_section *es;

	if ((es = elfx_get_section_by_addr(ef, addr)) == NULL)
		return (NULL);
	return ((char *)es->ptr + addr - es->baddr);
}

/*
 * Look up a symbol by name.
 */
elfx_symbol *
elfx_get_symbol_by_name(elfx_file *ef, const char *name)
{
	elfx_symbol *es;
	int lo, hi, mid;
	int cmp;

	if (ef->last_symbol_by_name != 0 &&
	    strcmp(name, ef->last_symbol_by_name->name) == 0)
		return (ef->last_symbol_by_name);
	es = ef->symbols;
	lo = 0;
	hi = ef->nsymbols - 1;
	for (;;) {
		mid = (lo + hi) / 2;
//		verbose("(%d, %d, %d) %s == %s",
//		    lo, hi, mid, es[mid].name, name);
		if ((cmp = strcmp(name, es[mid].name)) == 0)
			return (ef->last_symbol_by_name = &es[mid]);
		else if (lo == hi)
			return (NULL);
		else if (cmp < 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
}