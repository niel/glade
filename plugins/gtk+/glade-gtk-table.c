/*
 * glade-gtk-table.c - GladeWidgetAdaptor for GtkTable widget
 *
 * Copyright (C) 2008 Tristan Van Berkom
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-fixed.h"

typedef struct
{
  /* comparable part: */
  GladeWidget *widget;
  gint left_attach;
  gint right_attach;
  gint top_attach;
  gint bottom_attach;
} GladeGtkTableChild;

typedef enum
{
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
} GladeTableDir;

static GladeGtkTableChild table_edit = { 0, };
static GladeGtkTableChild table_cur_attach = { 0, };

static void
glade_gtk_table_get_child_attachments (GtkWidget * table,
                                       GtkWidget * child,
                                       GtkTableChild * tchild)
{
  guint left, right, top, bottom;

  gtk_container_child_get (GTK_CONTAINER (table), child,
                           "left-attach", (guint *) & left,
                           "right-attach", (guint *) & right,
                           "bottom-attach", (guint *) & bottom,
                           "top-attach", (guint *) & top, NULL);

  tchild->widget = child;
  tchild->left_attach = left;
  tchild->right_attach = right;
  tchild->top_attach = top;
  tchild->bottom_attach = bottom;
}


/* Takes a point (x or y depending on 'row') relative to
 * table, and returns the row or column in which the point
 * was found.
 */
static gint
glade_gtk_table_get_row_col_from_point (GtkTable * table,
                                        gboolean row, gint point)
{
  GtkTableChild tchild;
  GtkAllocation allocation;
  GList *list, *children;
  gint span, trans_point, size, base, end;

  children = gtk_container_get_children (GTK_CONTAINER (table));

  for (list = children; list; list = list->next)
    {
      glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                             GTK_WIDGET (list->data), &tchild);

      if (row)
        gtk_widget_translate_coordinates
            (GTK_WIDGET (table), tchild.widget, 0, point, NULL, &trans_point);
      else
        gtk_widget_translate_coordinates
            (GTK_WIDGET (table), tchild.widget, point, 0, &trans_point, NULL);

      gtk_widget_get_allocation (tchild.widget, &allocation);
      /* Find any widget in our row/column
       */
      end = row ? allocation.height : allocation.width;

      if (trans_point >= 0 &&
          /* should be trans_point < end ... test FIXME ! */
          trans_point < end)
        {
          base = row ? tchild.top_attach : tchild.left_attach;
          size = row ? allocation.height : allocation.width;
          span = row ? (tchild.bottom_attach - tchild.top_attach) :
              (tchild.right_attach - tchild.left_attach);

          return base + (trans_point * span / size);
        }
    }
  g_list_free (children);

  return -1;
}


static gboolean
glade_gtk_table_point_crosses_threshold (GtkTable * table,
                                         gboolean row,
                                         gint num,
                                         GladeTableDir dir, gint point)
{

  GtkTableChild tchild;
  GtkAllocation allocation;
  GList *list, *children;
  gint span, trans_point, size, rowcol_size, base;

  children = gtk_container_get_children (GTK_CONTAINER (table));

  for (list = children; list; list = list->next)
    {
      glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                             GTK_WIDGET (list->data), &tchild);

      /* Find any widget in our row/column
       */
      if ((row && num >= tchild.top_attach && num < tchild.bottom_attach) ||
          (!row && num >= tchild.left_attach && num < tchild.right_attach))
        {

          if (row)
            gtk_widget_translate_coordinates
                (GTK_WIDGET (table), tchild.widget,
                 0, point, NULL, &trans_point);
          else
            gtk_widget_translate_coordinates
                (GTK_WIDGET (table), tchild.widget,
                 point, 0, &trans_point, NULL);

          span = row ? (tchild.bottom_attach - tchild.top_attach) :
              (tchild.right_attach - tchild.left_attach);
          gtk_widget_get_allocation (tchild.widget, &allocation);
          size = row ? allocation.height : allocation.width;

          base = row ? tchild.top_attach : tchild.left_attach;
          rowcol_size = size / span;
          trans_point -= (num - base) * rowcol_size;

#if 0
          g_print ("dir: %s, widget size: %d, rowcol size: %d, "
                   "requested rowcol: %d, widget base rowcol: %d, trim: %d, "
                   "widget point: %d, thresh: %d\n",
                   dir == DIR_UP ? "up" : dir == DIR_DOWN ? "down" :
                   dir == DIR_LEFT ? "left" : "right",
                   size, rowcol_size, num, base, (num - base) * rowcol_size,
                   trans_point,
                   dir == DIR_UP || dir == DIR_LEFT ?
                   (rowcol_size / 2) : (rowcol_size / 2));
#endif
          switch (dir)
            {
              case DIR_UP:
              case DIR_LEFT:
                return trans_point <= (rowcol_size / 2);
              case DIR_DOWN:
              case DIR_RIGHT:
                return trans_point >= (rowcol_size / 2);
              default:
                break;
            }
        }

    }

  g_list_free (children);

  return FALSE;
}

static gboolean
glade_gtk_table_get_attachments (GladeFixed * fixed,
                                 GtkTable * table,
                                 GdkRectangle * rect,
                                 GladeGtkTableChild * configure)
{
  gint center_x, center_y, row, column;
  guint n_columns, n_rows;
  center_x = rect->x + (rect->width / 2);
  center_y = rect->y + (rect->height / 2);

  column = glade_gtk_table_get_row_col_from_point (table, FALSE, center_x);

  row = glade_gtk_table_get_row_col_from_point (table, TRUE, center_y);

  /* its a start, now try to grow when the rect extents
   * reach at least half way into the next row/column 
   */
  configure->left_attach = column;
  configure->right_attach = column + 1;
  configure->top_attach = row;
  configure->bottom_attach = row + 1;

  if (column >= 0 && row >= 0)
    {

      g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);

      /* Check and expand left
       */
      while (configure->left_attach > 0)
        {
          if (rect->x < fixed->child_x_origin &&
              fixed->operation != GLADE_CURSOR_DRAG &&
              GLADE_FIXED_CURSOR_LEFT (fixed->operation) == FALSE)
            break;

          if (glade_gtk_table_point_crosses_threshold
              (table, FALSE, configure->left_attach - 1,
               DIR_LEFT, rect->x) == FALSE)
            break;

          configure->left_attach--;
        }

      /* Check and expand right
       */
      while (configure->right_attach < n_columns)
        {
          if (rect->x + rect->width >
              fixed->child_x_origin + fixed->child_width_origin &&
              fixed->operation != GLADE_CURSOR_DRAG &&
              GLADE_FIXED_CURSOR_RIGHT (fixed->operation) == FALSE)
            break;

          if (glade_gtk_table_point_crosses_threshold
              (table, FALSE, configure->right_attach,
               DIR_RIGHT, rect->x + rect->width) == FALSE)
            break;

          configure->right_attach++;
        }

      /* Check and expand top
       */
      while (configure->top_attach > 0)
        {
          if (rect->y < fixed->child_y_origin &&
              fixed->operation != GLADE_CURSOR_DRAG &&
              GLADE_FIXED_CURSOR_TOP (fixed->operation) == FALSE)
            break;

          if (glade_gtk_table_point_crosses_threshold
              (table, TRUE, configure->top_attach - 1,
               DIR_UP, rect->y) == FALSE)
            break;

          configure->top_attach--;
        }

      /* Check and expand bottom
       */
      while (configure->bottom_attach < n_rows)
        {
          if (rect->y + rect->height >
              fixed->child_y_origin + fixed->child_height_origin &&
              fixed->operation != GLADE_CURSOR_DRAG &&
              GLADE_FIXED_CURSOR_BOTTOM (fixed->operation) == FALSE)
            break;

          if (glade_gtk_table_point_crosses_threshold
              (table, TRUE, configure->bottom_attach,
               DIR_DOWN, rect->y + rect->height) == FALSE)
            break;

          configure->bottom_attach++;
        }
    }

  /* Keep the same row/col span when performing a drag
   */
  if (fixed->operation == GLADE_CURSOR_DRAG)
    {
      gint col_span = table_edit.right_attach - table_edit.left_attach;
      gint row_span = table_edit.bottom_attach - table_edit.top_attach;

      if (rect->x < fixed->child_x_origin)
        configure->right_attach = configure->left_attach + col_span;
      else
        configure->left_attach = configure->right_attach - col_span;

      if (rect->y < fixed->child_y_origin)
        configure->bottom_attach = configure->top_attach + row_span;
      else
        configure->top_attach = configure->bottom_attach - row_span;
    }
  else if (fixed->operation == GLADE_CURSOR_RESIZE_RIGHT)
    {
      configure->left_attach = table_edit.left_attach;
      configure->top_attach = table_edit.top_attach;
      configure->bottom_attach = table_edit.bottom_attach;
    }
  else if (fixed->operation == GLADE_CURSOR_RESIZE_LEFT)
    {
      configure->right_attach = table_edit.right_attach;
      configure->top_attach = table_edit.top_attach;
      configure->bottom_attach = table_edit.bottom_attach;
    }
  else if (fixed->operation == GLADE_CURSOR_RESIZE_TOP)
    {
      configure->left_attach = table_edit.left_attach;
      configure->right_attach = table_edit.right_attach;
      configure->bottom_attach = table_edit.bottom_attach;
    }
  else if (fixed->operation == GLADE_CURSOR_RESIZE_BOTTOM)
    {
      configure->left_attach = table_edit.left_attach;
      configure->right_attach = table_edit.right_attach;
      configure->top_attach = table_edit.top_attach;
    }

  return column >= 0 && row >= 0;
}

static gboolean
glade_gtk_table_configure_child (GladeFixed * fixed,
                                 GladeWidget * child,
                                 GdkRectangle * rect, GtkWidget * table)
{
  GladeGtkTableChild configure = { child, };

  /* Sometimes we are unable to find a widget in the appropriate column,
   * usually because a placeholder hasnt had its size allocation yet.
   */
  if (glade_gtk_table_get_attachments
      (fixed, GTK_TABLE (table), rect, &configure))
    {
      if (memcmp (&configure, &table_cur_attach, sizeof (GladeGtkTableChild)) != 0)
        {

          glade_property_push_superuser ();
          glade_widget_pack_property_set (child, "left-attach",
                                          configure.left_attach);
          glade_widget_pack_property_set (child, "right-attach",
                                          configure.right_attach);
          glade_widget_pack_property_set (child, "top-attach",
                                          configure.top_attach);
          glade_widget_pack_property_set (child, "bottom-attach",
                                          configure.bottom_attach);
          glade_property_pop_superuser ();

          memcpy (&table_cur_attach, &configure, sizeof (GladeGtkTableChild));
        }
    }
  return TRUE;
}


static gboolean
glade_gtk_table_configure_begin (GladeFixed * fixed,
                                 GladeWidget * child, GtkWidget * table)
{

  table_edit.widget = child;

  glade_widget_pack_property_get (child, "left-attach",
                                  &table_edit.left_attach);
  glade_widget_pack_property_get (child, "right-attach",
                                  &table_edit.right_attach);
  glade_widget_pack_property_get (child, "top-attach", &table_edit.top_attach);
  glade_widget_pack_property_get (child, "bottom-attach",
                                  &table_edit.bottom_attach);

  memcpy (&table_cur_attach, &table_edit, sizeof (GladeGtkTableChild));

  return TRUE;
}

static gboolean
glade_gtk_table_configure_end (GladeFixed * fixed,
                               GladeWidget * child, GtkWidget * table)
{
  GladeGtkTableChild new_child = { child, };

  glade_widget_pack_property_get (child, "left-attach", &new_child.left_attach);
  glade_widget_pack_property_get (child, "right-attach",
                                  &new_child.right_attach);
  glade_widget_pack_property_get (child, "top-attach", &new_child.top_attach);
  glade_widget_pack_property_get (child, "bottom-attach",
                                  &new_child.bottom_attach);

  /* Compare the meaningfull part of the current edit. */
  if (memcmp (&new_child, &table_edit, sizeof (GladeGtkTableChild)) != 0)
    {
      GValue left_attach_value = { 0, };
      GValue right_attach_value = { 0, };
      GValue top_attach_value = { 0, };
      GValue bottom_attach_value = { 0, };

      GValue new_left_attach_value = { 0, };
      GValue new_right_attach_value = { 0, };
      GValue new_top_attach_value = { 0, };
      GValue new_bottom_attach_value = { 0, };

      GladeProperty *left_attach_prop, *right_attach_prop,
          *top_attach_prop, *bottom_attach_prop;

      left_attach_prop = glade_widget_get_pack_property (child, "left-attach");
      right_attach_prop =
          glade_widget_get_pack_property (child, "right-attach");
      top_attach_prop = glade_widget_get_pack_property (child, "top-attach");
      bottom_attach_prop =
          glade_widget_get_pack_property (child, "bottom-attach");

      g_return_val_if_fail (GLADE_IS_PROPERTY (left_attach_prop), FALSE);
      g_return_val_if_fail (GLADE_IS_PROPERTY (right_attach_prop), FALSE);
      g_return_val_if_fail (GLADE_IS_PROPERTY (top_attach_prop), FALSE);
      g_return_val_if_fail (GLADE_IS_PROPERTY (bottom_attach_prop), FALSE);

      glade_property_get_value (left_attach_prop, &new_left_attach_value);
      glade_property_get_value (right_attach_prop, &new_right_attach_value);
      glade_property_get_value (top_attach_prop, &new_top_attach_value);
      glade_property_get_value (bottom_attach_prop, &new_bottom_attach_value);

      g_value_init (&left_attach_value, G_TYPE_UINT);
      g_value_init (&right_attach_value, G_TYPE_UINT);
      g_value_init (&top_attach_value, G_TYPE_UINT);
      g_value_init (&bottom_attach_value, G_TYPE_UINT);

      g_value_set_uint (&left_attach_value, table_edit.left_attach);
      g_value_set_uint (&right_attach_value, table_edit.right_attach);
      g_value_set_uint (&top_attach_value, table_edit.top_attach);
      g_value_set_uint (&bottom_attach_value, table_edit.bottom_attach);

      glade_command_push_group (_("Placing %s inside %s"),
                                glade_widget_get_name (child), 
				glade_widget_get_name (GLADE_WIDGET (fixed)));
      glade_command_set_properties
          (left_attach_prop, &left_attach_value, &new_left_attach_value,
           right_attach_prop, &right_attach_value, &new_right_attach_value,
           top_attach_prop, &top_attach_value, &new_top_attach_value,
           bottom_attach_prop, &bottom_attach_value, &new_bottom_attach_value,
           NULL);
      glade_command_pop_group ();

      g_value_unset (&left_attach_value);
      g_value_unset (&right_attach_value);
      g_value_unset (&top_attach_value);
      g_value_unset (&bottom_attach_value);
      g_value_unset (&new_left_attach_value);
      g_value_unset (&new_right_attach_value);
      g_value_unset (&new_top_attach_value);
      g_value_unset (&new_bottom_attach_value);
    }

  return TRUE;
}

void
glade_gtk_table_post_create (GladeWidgetAdaptor * adaptor,
                             GObject * container, GladeCreateReason reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  g_signal_connect (G_OBJECT (gwidget), "configure-child",
                    G_CALLBACK (glade_gtk_table_configure_child), container);

  g_signal_connect (G_OBJECT (gwidget), "configure-begin",
                    G_CALLBACK (glade_gtk_table_configure_begin), container);

  g_signal_connect (G_OBJECT (gwidget), "configure-end",
                    G_CALLBACK (glade_gtk_table_configure_end), container);
}

static gboolean
glade_gtk_table_has_child (GtkTable * table, guint left_attach,
                           guint top_attach)
{
  GList *list, *children;
  gboolean ret = FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (table));

  for (list = children; list && list->data; list = list->next)
    {
      GtkTableChild child;

      glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                             GTK_WIDGET (list->data), &child);

      if (left_attach >= child.left_attach && left_attach < child.right_attach
          && top_attach >= child.top_attach && top_attach < child.bottom_attach)
        {
          ret = TRUE;
          break;
        }
    }

  g_list_free (children);

  return ret;
}

static gboolean
glade_gtk_table_widget_exceeds_bounds (GtkTable * table, gint n_rows,
                                       gint n_cols)
{
  GList *list, *children;
  gboolean ret = FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (table));

  for (list = children; list && list->data; list = list->next)
    {
      GtkTableChild child;

      glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                             GTK_WIDGET (list->data), &child);

      if (GLADE_IS_PLACEHOLDER (child.widget) == FALSE &&
          (child.right_attach > n_cols || child.bottom_attach > n_rows))
        {
          ret = TRUE;
          break;
        }
    }

  g_list_free (children);

  return ret;
}

#define TABLE_OCCUPIED(occmap, n_columns, col, row) \
    (occmap)[row * n_columns + col]

static void
glade_gtk_table_build_occupation_maps(GtkTable *table, guint n_columns, guint n_rows,
				      gchar **child_map, gpointer **placeholder_map)
{
    guint i, j;
    GList *list, *children = gtk_container_get_children (GTK_CONTAINER (table));

    *child_map = g_malloc0(n_columns * n_rows * sizeof(gchar));  /* gchar is smaller than gboolean */
    *placeholder_map = g_malloc0(n_columns * n_rows * sizeof(gpointer));

    for (list = children; list && list->data; list = list->next)
    {
	GtkTableChild child;

	glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
					       GTK_WIDGET (list->data), &child);

	if (GLADE_IS_PLACEHOLDER(list->data))
	{
	    /* assumption: placeholders are always attached to exactly 1 cell */
	    TABLE_OCCUPIED(*placeholder_map, n_columns, child.left_attach, child.top_attach) = list->data;
	}
	else
	{
	    for (i = child.left_attach; i < child.right_attach && i < n_columns; i++)
	    {
		for (j = child.top_attach; j < child.bottom_attach && j < n_rows; j++)
		{
		    TABLE_OCCUPIED(*child_map, n_columns, i, j) = 1;
		}
	    }
	}
    }
    g_list_free (children);
}

static void
glade_gtk_table_refresh_placeholders (GtkTable * table)
{
  guint n_columns, n_rows, i, j;
  gchar *child_map;
  gpointer *placeholder_map;

  g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);
  glade_gtk_table_build_occupation_maps (table, n_columns, n_rows,
					 &child_map, &placeholder_map);

  for (i = 0; i < n_columns; i++)
    {
      for (j = 0; j < n_rows; j++)
	{
	  gpointer placeholder = TABLE_OCCUPIED(placeholder_map, n_columns, i, j);

	  if (TABLE_OCCUPIED(child_map, n_columns, i, j))
	    {
	      if (placeholder)
		{
		  gtk_container_remove (GTK_CONTAINER (table), 
					GTK_WIDGET (placeholder));
		}
	    }
	  else
	    {
	      if (!placeholder)
		{
		  gtk_table_attach_defaults (table, 
					     glade_placeholder_new (), 
					     i, i + 1, j, j + 1);
		}
	    }
	}
    }
  g_free(child_map);
  g_free(placeholder_map);
  gtk_container_check_resize (GTK_CONTAINER (table));
}

static void
gtk_table_children_callback (GtkWidget * widget, gpointer client_data)
{
  GList **children;

  children = (GList **) client_data;
  *children = g_list_prepend (*children, widget);
}

GList *
glade_gtk_table_get_children (GladeWidgetAdaptor * adaptor,
                              GtkContainer * container)
{
  GList *children = NULL;

  g_return_val_if_fail (GTK_IS_TABLE (container), NULL);

  gtk_container_forall (container, gtk_table_children_callback, &children);

  /* GtkTable has the children list already reversed */
  return children;
}

void
glade_gtk_table_add_child (GladeWidgetAdaptor * adaptor,
                           GObject * object, GObject * child)
{
  g_return_if_fail (GTK_IS_TABLE (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void
glade_gtk_table_remove_child (GladeWidgetAdaptor * adaptor,
                              GObject * object, GObject * child)
{
  g_return_if_fail (GTK_IS_TABLE (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void
glade_gtk_table_replace_child (GladeWidgetAdaptor * adaptor,
                               GtkWidget * container,
                               GtkWidget * current, GtkWidget * new_widget)
{
  g_return_if_fail (GTK_IS_TABLE (container));
  g_return_if_fail (GTK_IS_WIDGET (current));
  g_return_if_fail (GTK_IS_WIDGET (new_widget));

  /* Chain Up */
  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                           G_OBJECT (container),
                                           G_OBJECT (current),
                                           G_OBJECT (new_widget));

  /* If we are replacing a GladeWidget, we must refresh placeholders
   * because the widget may have spanned multiple rows/columns, we must
   * not do so in the case we are pasting multiple widgets into a table,
   * where destroying placeholders results in default packing properties
   * (since the remaining placeholder templates no longer exist, only the
   * first pasted widget would have proper packing properties).
   */
  if (glade_widget_get_from_gobject (new_widget) == NULL)
    glade_gtk_table_refresh_placeholders (GTK_TABLE (container));

}

static void
glade_gtk_table_set_n_common (GObject * object, const GValue * value,
                              gboolean for_rows)
{
  GladeWidget *widget;
  GtkTable *table;
  guint new_size, old_size, n_columns, n_rows;

  table = GTK_TABLE (object);
  g_return_if_fail (GTK_IS_TABLE (table));

  g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);

  new_size = g_value_get_uint (value);
  old_size = for_rows ? n_rows : n_columns;

  if (new_size < 1)
    return;

  if (glade_gtk_table_widget_exceeds_bounds
      (table, for_rows ? new_size : n_rows, for_rows ? n_columns : new_size))
    /* Refuse to shrink if it means orphaning widgets */
    return;

  widget = glade_widget_get_from_gobject (GTK_WIDGET (table));
  g_return_if_fail (widget != NULL);

  if (for_rows)
    gtk_table_resize (table, new_size, n_columns);
  else
    gtk_table_resize (table, n_rows, new_size);

  /* Fill table with placeholders */
  glade_gtk_table_refresh_placeholders (table);

  if (new_size < old_size)
    {
      /* Remove from the bottom up */
      GList *list, *children;
      GList *list_to_free = NULL;

      children = gtk_container_get_children (GTK_CONTAINER (table));

      for (list = children; list && list->data; list = list->next)
        {
          GtkTableChild child;
          guint start, end;

          glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                                 GTK_WIDGET (list->data),
                                                 &child);

          start = for_rows ? child.top_attach : child.left_attach;
          end = for_rows ? child.bottom_attach : child.right_attach;

          /* We need to completely remove it */
          if (start >= new_size)
            {
              list_to_free = g_list_prepend (list_to_free, child.widget);
              continue;
            }

          /* If the widget spans beyond the new border,
           * we should resize it to fit on the new table */
          if (end > new_size)
            gtk_container_child_set
                (GTK_CONTAINER (table), GTK_WIDGET (child.widget),
                 for_rows ? "bottom_attach" : "right_attach", new_size, NULL);
        }

      g_list_free (children);

      if (list_to_free)
        {
          for (list = g_list_first (list_to_free);
               list && list->data; list = list->next)
            {
              g_object_ref (G_OBJECT (list->data));
              gtk_container_remove (GTK_CONTAINER (table),
                                    GTK_WIDGET (list->data));
              /* This placeholder is no longer valid, force destroy */
              gtk_widget_destroy (GTK_WIDGET (list->data));
            }
          g_list_free (list_to_free);
        }
      gtk_table_resize (table,
                        for_rows ? new_size : n_rows,
                        for_rows ? n_columns : new_size);
    }
}

void
glade_gtk_table_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id, const GValue * value)
{
  if (!strcmp (id, "n-rows"))
    glade_gtk_table_set_n_common (object, value, TRUE);
  else if (!strcmp (id, "n-columns"))
    glade_gtk_table_set_n_common (object, value, FALSE);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
                                                      id, value);
}

static gboolean
glade_gtk_table_verify_n_common (GObject * object, const GValue * value,
                                 gboolean for_rows)
{
  GtkTable *table = GTK_TABLE (object);
  guint n_columns, n_rows, new_size = g_value_get_uint (value);

  g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);

  if (glade_gtk_table_widget_exceeds_bounds
      (table, for_rows ? new_size : n_rows, for_rows ? n_columns : new_size))
    /* Refuse to shrink if it means orphaning widgets */
    return FALSE;

  return TRUE;
}

gboolean
glade_gtk_table_verify_property (GladeWidgetAdaptor * adaptor,
                                 GObject * object,
                                 const gchar * id, const GValue * value)
{
  if (!strcmp (id, "n-rows"))
    return glade_gtk_table_verify_n_common (object, value, TRUE);
  else if (!strcmp (id, "n-columns"))
    return glade_gtk_table_verify_n_common (object, value, FALSE);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                         id, value);

  return TRUE;
}

void
glade_gtk_table_set_child_property (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * child,
                                    const gchar * property_name, GValue * value)
{
  g_return_if_fail (GTK_IS_TABLE (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL && value != NULL);

  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                container, child,
                                                property_name, value);

  if (strcmp (property_name, "bottom-attach") == 0 ||
      strcmp (property_name, "left-attach") == 0 ||
      strcmp (property_name, "right-attach") == 0 ||
      strcmp (property_name, "top-attach") == 0)
    {
      /* Refresh placeholders */
      glade_gtk_table_refresh_placeholders (GTK_TABLE (container));
    }

}

static gboolean
glade_gtk_table_verify_attach_common (GObject * object,
                                      GValue * value,
                                      guint * val,
                                      const gchar * prop,
                                      guint * prop_val,
                                      const gchar * parent_prop,
                                      guint * parent_val)
{
  GladeWidget *widget, *parent;

  widget = glade_widget_get_from_gobject (object);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), TRUE);
  parent = glade_widget_get_parent (widget);
  g_return_val_if_fail (GLADE_IS_WIDGET (parent), TRUE);

  *val = g_value_get_uint (value);
  glade_widget_property_get (widget, prop, prop_val);
  glade_widget_property_get (parent, parent_prop, parent_val);

  return FALSE;
}

static gboolean
glade_gtk_table_verify_left_top_attach (GObject * object,
                                        GValue * value,
                                        const gchar * prop,
                                        const gchar * parent_prop)
{
  guint val, prop_val, parent_val;

  if (glade_gtk_table_verify_attach_common (object, value, &val,
                                            prop, &prop_val,
                                            parent_prop, &parent_val))
    return FALSE;

  if (val >= parent_val || val >= prop_val)
    return FALSE;

  return TRUE;
}

static gboolean
glade_gtk_table_verify_right_bottom_attach (GObject * object,
                                            GValue * value,
                                            const gchar * prop,
                                            const gchar * parent_prop)
{
  guint val, prop_val, parent_val;

  if (glade_gtk_table_verify_attach_common (object, value, &val,
                                            prop, &prop_val,
                                            parent_prop, &parent_val))
    return FALSE;

  if (val <= prop_val || val > parent_val)
    return FALSE;

  return TRUE;
}

gboolean
glade_gtk_table_child_verify_property (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * child,
                                       const gchar * id, GValue * value)
{
  if (!strcmp (id, "left-attach"))
    return glade_gtk_table_verify_left_top_attach (child,
                                                   value,
                                                   "right-attach", "n-columns");
  else if (!strcmp (id, "right-attach"))
    return glade_gtk_table_verify_right_bottom_attach (child,
                                                       value,
                                                       "left-attach",
                                                       "n-columns");
  else if (!strcmp (id, "top-attach"))
    return glade_gtk_table_verify_left_top_attach (child,
                                                   value,
                                                   "bottom-attach", "n-rows");
  else if (!strcmp (id, "bottom-attach"))
    return glade_gtk_table_verify_right_bottom_attach (child,
                                                       value,
                                                       "top-attach", "n-rows");
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_verify_property)
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_verify_property (adaptor,
                                                     container, child,
                                                     id, value);

  return TRUE;
}

static void
glade_gtk_table_child_insert_remove_action (GladeWidgetAdaptor *adaptor, 
					    GObject            *container, 
					    GObject            *object, 
					    const gchar        *group_format, 
					    const gchar        *n_row_col, 
					    const gchar        *attach1,    /* should be smaller (top/left) attachment */
                                            const gchar        *attach2,      /* should be larger (bot/right) attachment */
                                            gboolean            remove, 
					    gboolean            after)
{
  GladeWidget *parent;
  GList *children, *l;
  gint child_pos, size, offset;

  gtk_container_child_get (GTK_CONTAINER (container),
                           GTK_WIDGET (object),
                           after ? attach2 : attach1, &child_pos, NULL);

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (group_format, glade_widget_get_name (parent));

  children = glade_widget_adaptor_get_children (adaptor, container);
  /* Make sure widgets does not get destroyed */
  g_list_foreach (children, (GFunc) g_object_ref, NULL);

  glade_widget_property_get (parent, n_row_col, &size);

  if (remove)
    {
      GList *del = NULL;
      /* Remove children first */
      for (l = children; l; l = g_list_next (l))
        {
          GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
          gint pos1, pos2;

          /* Skip placeholders */
          if (gchild == NULL)
            continue;

          glade_widget_pack_property_get (gchild, attach1, &pos1);
          glade_widget_pack_property_get (gchild, attach2, &pos2);
          if ((pos1 + 1 == pos2) && ((after ? pos2 : pos1) == child_pos))
            {
              del = g_list_prepend (del, gchild);
            }
        }
      if (del)
        {
          glade_command_delete (del);
          g_list_free (del);
        }
      offset = -1;
    }
  else
    {
      /* Expand the table */
      glade_command_set_property (glade_widget_get_property (parent, n_row_col),
                                  size + 1);
      offset = 1;
    }

  /* Reorder children */
  for (l = children; l; l = g_list_next (l))
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
      gint pos;

      /* Skip placeholders */
      if (gchild == NULL)
        continue;

      /* if removing, do top/left before bot/right */
      if (remove)
        {
          /* adjust top-left attachment */
          glade_widget_pack_property_get (gchild, attach1, &pos);
          if (pos > child_pos || (after && pos == child_pos))
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach1), pos + offset);
            }

          /* adjust bottom-right attachment */
          glade_widget_pack_property_get (gchild, attach2, &pos);
          if (pos > child_pos || (after && pos == child_pos))
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach2), pos + offset);
            }

        }
      /* if inserting, do bot/right before top/left */
      else
        {
          /* adjust bottom-right attachment */
          glade_widget_pack_property_get (gchild, attach2, &pos);
          if (pos > child_pos)
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach2), pos + offset);
            }

          /* adjust top-left attachment */
          glade_widget_pack_property_get (gchild, attach1, &pos);
          if (pos >= child_pos)
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach1), pos + offset);
            }
        }
    }

  if (remove)
    {
      /* Shrink the table */
      glade_command_set_property (glade_widget_get_property (parent, n_row_col),
                                  size - 1);
    }

  g_list_foreach (children, (GFunc) g_object_unref, NULL);
  g_list_free (children);

  glade_command_pop_group ();
}

void
glade_gtk_table_child_action_activate (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * object,
                                       const gchar * action_path)
{
  if (strcmp (action_path, "insert_row/after") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Row on %s"),
                                                  "n-rows", "top-attach",
                                                  "bottom-attach", FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_row/before") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Row on %s"),
                                                  "n-rows", "top-attach",
                                                  "bottom-attach",
                                                  FALSE, FALSE);
    }
  else if (strcmp (action_path, "insert_column/after") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Column on %s"),
                                                  "n-columns", "left-attach",
                                                  "right-attach", FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_column/before") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Column on %s"),
                                                  "n-columns", "left-attach",
                                                  "right-attach", FALSE, FALSE);
    }
  else if (strcmp (action_path, "remove_column") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  _("Remove Column on %s"),
                                                  "n-columns", "left-attach",
                                                  "right-attach", TRUE, FALSE);
    }
  else if (strcmp (action_path, "remove_row") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  _("Remove Row on %s"),
                                                  "n-rows", "top-attach",
                                                  "bottom-attach", TRUE, FALSE);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}