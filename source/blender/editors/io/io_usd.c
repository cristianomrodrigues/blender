/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_USD
#  include "DNA_space_types.h"

#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_report.h"

#  include "BLI_path_util.h"
#  include "BLI_string.h"

#  include "RNA_access.h"
#  include "RNA_define.h"

#  include "UI_interface.h"
#  include "UI_resources.h"

#  include "WM_api.h"
#  include "WM_types.h"

#  include "DEG_depsgraph.h"

#  include "io_usd.h"
#  include "usd.h"

const EnumPropertyItem rna_enum_usd_export_evaluation_mode_items[] = {
    {DAG_EVAL_RENDER,
     "RENDER",
     0,
     "Render",
     "Use Render settings for object visibility, modifier settings, etc"},
    {DAG_EVAL_VIEWPORT,
     "VIEWPORT",
     0,
     "Viewport",
     "Use Viewport settings for object visibility, modifier settings, etc"},
    {0, NULL, 0, NULL, NULL},
};

static int wm_usd_export_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "as_background_job")) {
    RNA_boolean_set(op->ptr, "as_background_job", true);
  }

  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    Main *bmain = CTX_data_main(C);
    char filepath[FILE_MAX];

    if (BKE_main_blendfile_path(bmain)[0] == '\0') {
      BLI_strncpy(filepath, "untitled", sizeof(filepath));
    }
    else {
      BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
    }

    BLI_path_extension_replace(filepath, sizeof(filepath), ".usdc");
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;

  UNUSED_VARS(event);
}

static int wm_usd_export_exec(bContext *C, wmOperator *op)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  char filename[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filename);

  /* Take some defaults from the scene, if not specified explicitly. */
  Scene *scene = CTX_data_scene(C);

  const bool as_background_job = RNA_boolean_get(op->ptr, "as_background_job");
  const bool selected_objects_only = RNA_boolean_get(op->ptr, "selected_objects_only");
  const bool visible_objects_only = RNA_boolean_get(op->ptr, "visible_objects_only");
  const bool export_animation = RNA_boolean_get(op->ptr, "export_animation");
  const bool export_hair = RNA_boolean_get(op->ptr, "export_hair");
  const bool export_uvmaps = RNA_boolean_get(op->ptr, "export_uvmaps");
  const bool export_normals = RNA_boolean_get(op->ptr, "export_normals");
  const bool export_materials = RNA_boolean_get(op->ptr, "export_materials");
  const bool use_instancing = RNA_boolean_get(op->ptr, "use_instancing");
  const bool evaluation_mode = RNA_enum_get(op->ptr, "evaluation_mode");

  struct USDExportParams params = {
      export_animation,
      export_hair,
      export_uvmaps,
      export_normals,
      export_materials,
      selected_objects_only,
      visible_objects_only,
      use_instancing,
      evaluation_mode,
  };

  bool ok = USD_export(scene, C, filename, &params, as_background_job);

  return as_background_job || ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void wm_usd_export_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *col;
  struct PointerRNA *ptr = op->ptr;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "selected_objects_only", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "visible_objects_only", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "export_animation", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_hair", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_uvmaps", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_normals", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "export_materials", 0, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_instancing", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "evaluation_mode", 0, NULL, ICON_NONE);
}

void WM_OT_usd_export(struct wmOperatorType *ot)
{
  ot->name = "Export USD";
  ot->description = "Export current scene in a USD archive";
  ot->idname = "WM_OT_usd_export";

  ot->invoke = wm_usd_export_invoke;
  ot->exec = wm_usd_export_exec;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_usd_export_draw;

  WM_operator_properties_filesel(
      ot, 0, FILE_BLENDER, FILE_SAVE, WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

  RNA_def_boolean(ot->srna,
                  "selected_objects_only",
                  false,
                  "Only Export Selected Objects",
                  "Only selected objects are exported. Unselected parents of selected objects are "
                  "exported as empty transform");

  RNA_def_boolean(ot->srna,
                  "visible_objects_only",
                  true,
                  "Only Export Visible Objects",
                  "Only visible objects are exported. Invisible parents of visible objects are "
                  "exported as empty transform");

  RNA_def_boolean(ot->srna,
                  "export_animation",
                  false,
                  "Export Animation",
                  "When checked, the render frame range is exported. When false, only the current "
                  "frame is exported");
  RNA_def_boolean(ot->srna,
                  "export_hair",
                  false,
                  "Export Hair",
                  "When checked, hair is exported as USD curves");
  RNA_def_boolean(ot->srna,
                  "export_uvmaps",
                  true,
                  "Export UV Maps",
                  "When checked, all UV maps of exported meshes are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_normals",
                  true,
                  "Export Normals",
                  "When checked, normals of exported meshes are included in the export");
  RNA_def_boolean(ot->srna,
                  "export_materials",
                  true,
                  "Export Materials",
                  "When checked, the viewport settings of materials are exported as USD preview "
                  "materials, and material assignments are exported as geometry subsets");

  RNA_def_boolean(ot->srna,
                  "use_instancing",
                  false,
                  "Use Instancing (EXPERIMENTAL)",
                  "When true, dupli-objects are written as instances of the original in USD. "
                  "Experimental feature, not working perfectly");

  RNA_def_enum(ot->srna,
               "evaluation_mode",
               rna_enum_usd_export_evaluation_mode_items,
               DAG_EVAL_RENDER,
               "Evaluation Mode",
               "Determines visibility of objects and modifier settings");

  RNA_def_boolean(
      ot->srna,
      "as_background_job",
      false,
      "Run as Background Job",
      "Enable this to run the import in the background, disable to block Blender while importing. "
      "This option is deprecated; EXECUTE this operator to run in the foreground, and INVOKE it "
      "to run as a background job");
}

#endif /* WITH_USD */