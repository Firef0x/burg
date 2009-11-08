/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/uitree.h>

GRUB_EXPORT (grub_uitree_dump);
GRUB_EXPORT (grub_uitree_find);
GRUB_EXPORT (grub_uitree_find_id);
GRUB_EXPORT (grub_uitree_create_node);
GRUB_EXPORT (grub_uitree_load);
GRUB_EXPORT (grub_uitree_load_string);
GRUB_EXPORT (grub_uitree_load_file);
GRUB_EXPORT (grub_uitree_free);
GRUB_EXPORT (grub_uitree_root);
GRUB_EXPORT (grub_uitree_set_prop);
GRUB_EXPORT (grub_uitree_get_prop);

struct grub_uitree grub_uitree_root;

#define GRUB_UITREE_TOKEN_MAXLEN	1024

#define TOKEN_TYPE_ERROR	0
#define TOKEN_TYPE_STRING	1
#define TOKEN_TYPE_EQUAL	2
#define TOKEN_TYPE_LBRACKET	3
#define TOKEN_TYPE_RBRACKET	4

static int
read_token (char *buf, char *pre, int size, int *count)
{
  int escape = 0;
  int string = 0;
  int comment = 0;
  int pos = 0, ofs = 0;

  *count = 0;
  if (*pre)
    {
      char c = *pre;

      *pre = 0;
      if (c == '=')
	return TOKEN_TYPE_EQUAL;
      else if (c == '{')
	return TOKEN_TYPE_LBRACKET;
      else
	return TOKEN_TYPE_RBRACKET;
    }

  while (1)
    {
      char c;

      if (ofs >= size)
	return 0;

      c = buf[ofs++];

      if (c == '\r')
	continue;

      if (c == '\t')
	c = ' ';

      if (comment)
	{
	  if (c == '\n')
	    {
	      comment = 0;
	      if (pos)
		break;
	    }
	  continue;
	}

      if (string)
	{
	  if (escape)
	    {
	      escape = 0;

	      if (c == 'n')
		c = '\n';
	    }
	  else
	    {
	      if (c == '\"')
		break;

	      if (c == '\\')
		{
		  escape = 1;
		  continue;
		}
	    }
	}
      else
	{
	  if ((c == ' ') || (c == '\n'))
	    {
	      if (pos)
		break;
	      else
		continue;
	    }
	  else if (c == '\"')
	    {
	      string = 1;
	      continue;
	    }
	  else if (c == '#')
	    {
	      comment = 1;
	      continue;
	    }
	  else if ((c == '=') || (c == '{') || (c == '}'))
	    {
	      if (pos)
		*pre = c;
	      else
		{
		  *count = ofs;
		  if (c == '=')
		    return TOKEN_TYPE_EQUAL;
		  else if (c == '{')
		    return TOKEN_TYPE_LBRACKET;
		  else
		    return TOKEN_TYPE_RBRACKET;
		}

	      break;
	    }
	}

      buf[pos++] = c;
    }

  buf[pos] = 0;
  *count = ofs;
  return TOKEN_TYPE_STRING;
}

static void
grub_uitree_dump_padding (int level)
{
  while (level > 0)
    {
      grub_printf ("  ");
      level--;
    }
}

static void
grub_uitree_dump_node (grub_uitree_t node, int level)
{
  grub_uiprop_t p = node->prop;

  grub_uitree_dump_padding (level);
  grub_printf ("%s", node->name);
  while (p)
    {
      grub_printf (" %s=\"%s\"", p->name, p->value);
      p = p->next;
    }
  grub_printf ("\n");

  level++;
  node = node->child;
  while (node)
    {
      grub_uitree_dump_node (node, level);
      node = node->next;
    }
}

void
grub_uitree_dump (grub_uitree_t node)
{
  grub_uitree_dump_node (node, 0);
}

grub_uitree_t
grub_uitree_find (grub_uitree_t node, const char *name)
{
  node = node->child;
  while (node)
    {
      if (! grub_strcmp (node->name, name))
	break;
      node = node->next;
    }

  return node;
}

grub_uitree_t
grub_uitree_find_id (grub_uitree_t node, const char *name)
{
  grub_uitree_t child;

  child = node;
  while (child)
    {
      const char *id;

      id = grub_uitree_get_prop (child, "id");
      if ((id) && (! grub_strcmp (id, name)))
	return child;

      child = grub_tree_next_node (GRUB_AS_TREE (node), GRUB_AS_TREE (child));
    }
  return 0;
}

grub_uitree_t
grub_uitree_create_node (const char *name)
{
  grub_uitree_t n;

  n = grub_zalloc (sizeof (struct grub_uitree) + grub_strlen (name) + 1);
  if (! n)
    return 0;

  grub_strcpy (n->name, name);
  return n;
}

grub_uitree_t
grub_uitree_load (char *buf, int size, int *ret)
{
  int type, count;
  char pre, *save;
  grub_uitree_t node;

  save = buf;
  node = 0;
  pre = 0;
  while ((type = read_token (buf, &pre, size, &count)) != TOKEN_TYPE_ERROR)
    {
      if (type == TOKEN_TYPE_STRING)
	{
	  char *str;

	  str = buf;
	  buf += count;
	  size -= count;
	  type = read_token (buf, &pre, size, &count);
	  buf += count;
	  size -= count;
	  if (type == TOKEN_TYPE_EQUAL)
	    {
	      if (! node)
		break;

	      if (read_token (buf, &pre, size, &count) != TOKEN_TYPE_STRING)
		break;

	      if (grub_uitree_set_prop (node, str, buf))
		break;

	      buf += count;
	      size -= count;
	    }
	  else if (type == TOKEN_TYPE_LBRACKET)
	    {
	      grub_uitree_t n;

	      n = grub_uitree_create_node (str);
	      if (! n)
		break;

	      if (node)
		{
		  grub_tree_add_child (GRUB_AS_TREE (node), GRUB_AS_TREE (n),
				       -1);
		  node = n;
		}
	      else
		node = n;
	    }
	  else
	    break;
	}
      else if (type == TOKEN_TYPE_RBRACKET)
	{
	  buf += count;
	  size -= count;
	  if (! node->parent)
	    {
	      *ret = buf - save;
	      return node;
	    }

	  node = node->parent;
	}
      else
	break;
    }

  grub_uitree_free (node);
  return 0;
}

grub_uitree_t
grub_uitree_load_string (grub_uitree_t root, char *buf, int method)
{
  grub_uitree_t first = 0;
  int size;

  size = grub_strlen (buf);
  while (1)
    {
      grub_uitree_t node;
      int count;

      node = grub_uitree_load (buf, size, &count);
      if (! node)
	break;

      buf += count;
      size -= count;

      if (method <= GRUB_UITREE_LOAD_METHOD_APPEND)
	{
	  if (! first)
	    first = node;
	}
      else
	{
	  grub_uitree_t pre;

	  pre = grub_uitree_find (root, node->name);
	  if (pre)
	    {
	      if (pre->data)
		{
		  grub_uitree_free (node);
		  continue;
		}

	      if (method == GRUB_UITREE_LOAD_METHOD_MERGE)
		{
		  grub_uitree_t child, last;
		  grub_uiprop_t prop;

		  prop = node->prop;
		  while (prop)
		    {
		      grub_uiprop_t *ptr, cur, next;

		      next = 0;
		      ptr = &pre->prop;
		      while (*ptr)
			{
			  if (! grub_strcmp ((*ptr)->name, prop->name))
			    {
			      next = (*ptr)->next;
			      grub_free (*ptr);
			      break;
			    }
			  ptr = &((*ptr)->next);
			}

		      cur = prop;
		      prop = prop->next;
		      cur->next = next;
		      *ptr = cur;
		    }

		  last = 0;
		  child = node->child;
		  while (child)
		    {
		      grub_uitree_t c;

		      c = child;
		      child = child->next;
		      if (last)
			grub_tree_add_sibling (GRUB_AS_TREE (last),
					       GRUB_AS_TREE (c));
		      else
			grub_tree_add_child (GRUB_AS_TREE (pre),
					     GRUB_AS_TREE (c), -1);
		      last = c;
		    }

		  grub_free (node);
		  continue;
		}
	      grub_tree_remove_node (GRUB_AS_TREE (pre));
	      grub_uitree_free (pre);
	    }
	}
      grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
      if (method == GRUB_UITREE_LOAD_METHOD_SINGLE)
	break;
    }

  return first;
}

grub_uitree_t
grub_uitree_load_file (grub_uitree_t root, const char *name, int method)
{
  grub_uitree_t result = 0;
  grub_file_t file;
  int size;

  file = grub_file_open (name);
  if (! file)
    return 0;

  size = file->size;
  if (size)
    {
      char *buf;

      buf = grub_malloc (size + 1);
      if (buf)
	{
	  grub_file_read (file, buf, size);
	  buf[size] = 0;
	  result = grub_uitree_load_string (root, buf, method);
	}
      grub_free (buf);
    }
  grub_file_close (file);

  return result;
}

void
grub_uitree_free (grub_uitree_t node)
{
  grub_uitree_t child;
  grub_uiprop_t p;

  if (! node)
    return;

  child = node->child;
  while (child)
    {
      grub_uitree_t c;

      c = child;
      child = child->next;
      grub_uitree_free (c);
    }

  p = node->prop;
  while (p)
    {
      grub_uiprop_t c;

      c = p;
      p = p->next;
      grub_free (c);
    }

  grub_free (node);
}

grub_err_t
grub_uitree_set_prop (grub_uitree_t node, const char *name, const char *value)
{
  int ofs, len;
  grub_uiprop_t p, *ptr, next;

  ofs = grub_strlen (name) + 1;
  len = ofs + grub_strlen (value) + 1;

  next = 0;
  ptr = &node->prop;
  while (*ptr)
    {
      if (! grub_strcmp ((*ptr)->name, name))
	{
	  next = (*ptr)->next;
	  break;
	}
      ptr = &((*ptr)->next);
    }

  p = grub_realloc (*ptr, sizeof (struct grub_uiprop) + len);
  if (! p)
    return grub_errno;

  *ptr = p;
  p->next = next;
  grub_strcpy (p->name, name);
  grub_strcpy (p->name + ofs, value);
  p->value = &p->name[ofs];

  return grub_errno;
}

char *
grub_uitree_get_prop (grub_uitree_t node, const char *name)
{
  grub_uiprop_t prop = node->prop;

  while (prop)
    {
      if (! grub_strcmp (prop->name, name))
	return prop->value;
      prop = prop->next;
    }

  return 0;
}
