#include <stdlib.h>
//#include <glib-object.h>
//#include <glib/gthread.h>
#include <string>
#include <tinyxml.h>

#include "config.h"
#include "util.h"

#include "page.h"
#include "site.h"
/*用于在通过站点文件获取站点信息*/
//定义站点文件
#define SITEFILE   "site.xml"
//定义gtk窗口
extern GtkWidget *m_window;

static char *m_sitefile = NULL;
static TiXmlDocument *m_doc = NULL;//对应xml整个文档
//添加gtk组件
static GtkWidget *m_object = NULL;
static GtkWidget *m_vbox = NULL;
//static GtkWidget *m_toolbar = NULL;
static GtkTreeStore *m_treestore = NULL;
static GtkWidget *m_treeview = NULL;

enum {
    COL_ICON,
    COL_NAME,
    COL_CFG,
    NUM_COLS,
};

// 激活tree节点,用于tree显示
void on_treeview_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) 
{
	GValue value = {0,};
	//定义迭代，并将迭代定义为指向路径path
	GtkTreeIter iter;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(m_treestore), &iter, path);
	//初始化并定义value
	gtk_tree_model_get_value(GTK_TREE_MODEL(m_treestore), &iter, COL_CFG,  &value);
	//读取site.xml的文件信息
	cfg_t *cfg = (cfg_t*) g_value_get_pointer(&value);
	if (cfg) {
		page_ssh_create(cfg);
	}
	// 如果为文件夹节点，则展开/收缩此节点
	else {
		if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(m_treeview), path)) {
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(m_treeview), path);
		}
		else {
			gtk_tree_view_expand_to_path(GTK_TREE_VIEW(m_treeview), path);
		}
	}
}
//站点设置
int site_init()
{
	//初始化站点文件位置
	m_sitefile = get_res_path(SITEFILE);
	m_doc = new TiXmlDocument();

	// main container，主界面
	m_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	m_object = m_vbox;
	/*定义toolbar，暂时禁用
	// toolbar
	m_toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(m_toolbar), GTK_TOOLBAR_BOTH);
	gtk_box_pack_start(GTK_BOX(m_vbox), m_toolbar, FALSE, FALSE, 0);
	 */
	// tree_store
	m_treestore = gtk_tree_store_new(NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
	if (m_treestore == NULL) {
		return -1;
	}

	// tree_view
	m_treeview = gtk_tree_view_new();
	if (m_treeview == NULL) {
		return -1;
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(m_treeview), GTK_TREE_MODEL(m_treestore));
	//树节点，窗口显示配置
	gtk_box_pack_start(GTK_BOX(m_vbox), m_treeview, TRUE, TRUE, 0);
	//配置是否显示headers
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (m_treeview), TRUE);
	//配置树节点里面的站点可以双击打开
	g_signal_connect(GTK_WIDGET(m_treeview), "row-activated", G_CALLBACK(on_treeview_row_activated), NULL);
	//不影响功能，暂时禁用  
	//GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m_treeview));
	//gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
	//配置一个子tree的创建
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Site List");
	gtk_tree_view_append_column(GTK_TREE_VIEW(m_treeview), col);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col, renderer, FALSE);
	gtk_tree_view_column_add_attribute(col, renderer, "pixbuf", COL_ICON);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, FALSE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COL_NAME);

	return 0;
}
//删除站点文件，释放内存空间
void site_term()
{
    delete m_doc;

    free(m_sitefile);
}
//获取配置文件和图标文件
static void _append_node(TiXmlElement *ele, GtkTreeIter *iter)
{
    GtkTreeIter t;
    GtkTreeIter *p = NULL;

    if (m_doc->RootElement() == ele) {
        TiXmlElement *child;
        for (child = ele->FirstChildElement();
             child != NULL;
             child = child->NextSiblingElement()) {
            _append_node(child, p);
        }
    }
    else if (strcmp(ele->Value(), "dir") == 0) {
        gtk_tree_store_append(m_treestore, &t, iter);
        //获取图标所在路径
        char *tmp = get_res_path(ICON_DIR);
        GdkPixbuf* icon = gdk_pixbuf_new_from_file(tmp, NULL);
        free(tmp);
        gtk_tree_store_set(m_treestore, &t, COL_ICON, icon, -1);

        gtk_tree_store_set(m_treestore, &t, COL_NAME, ele->Attribute("name"), -1);
        p = &t;

        TiXmlElement *child;
        for (child = ele->FirstChildElement();
             child != NULL;
             child = child->NextSiblingElement()) {
            _append_node(child, p);
        }
    }
    else if (strcmp(ele->Value(), "site") == 0) {
        gtk_tree_store_append(m_treestore, &t, iter);

        char *tmp = get_res_path(ICON_SITE);
        GdkPixbuf* icon = gdk_pixbuf_new_from_file(tmp, NULL);
        free(tmp);
        gtk_tree_store_set(m_treestore, &t, COL_ICON, icon, -1);

        gtk_tree_store_set(m_treestore, &t, COL_NAME, ele->Attribute("name"), -1);

        cfg_t *cfg = (cfg_t*) malloc(sizeof(cfg_t));
        memset(cfg, 0x00, sizeof(cfg_t));
        if (ele->Attribute("name")) {
            strcpy(cfg->name, ele->Attribute("name"));
        }
        if (ele->Attribute("host")) {
            strcpy(cfg->host, ele->Attribute("host"));
        }
        if (ele->Attribute("port")) {
            strcpy(cfg->port, ele->Attribute("port"));
        }
        if (ele->Attribute("user")) {
            strcpy(cfg->user, ele->Attribute("user"));
        }
        if (ele->Attribute("pass")) {
            strcpy(cfg->pass, ele->Attribute("pass"));
        }

        // cmd,进行程序执行
        TiXmlElement *btn;
        int i;
        int j;
        for (btn = ele->FirstChildElement(), i=0;
             btn != NULL && i<BTN_MAX_COUNT;
             btn = btn->NextSiblingElement(), i++) {

            if (strcmp(btn->Value(), "btn") != 0) {
                continue;
            }

            if (btn->Attribute("name")) {
                strcpy(cfg->btn[i].name, btn->Attribute("name"));
            }

            TiXmlElement *cmd;
            for (cmd = btn->FirstChildElement(), j=0;
                 cmd != NULL && j<CMD_MAX_COUNT;
                 cmd = cmd->NextSiblingElement(), j++) {

                if (strcmp(cmd->Value(), "cmd") != 0) {
                    continue;
                }

                if (cmd->Attribute("name")) {
                    strcpy(cfg->btn[i].cmd[j].name, cmd->Attribute("name"));
                }
                if (cmd->Attribute("cmd")) {
                    strcpy(cfg->btn[i].cmd[j].str, cmd->Attribute("cmd"));
                }
            }
        }

        gtk_tree_store_set(m_treestore, &t, COL_CFG, cfg, -1);

        p = &t;
    }
}
//进行站点加载
int site_load()
{
		//判断，如果能正常访问站点文件的话
    if (access(m_sitefile, R_OK|W_OK) == 0) {
        // 清理旧的记录
        m_doc->Clear();
        gtk_tree_store_clear(GTK_TREE_STORE(m_treestore));  

        // 打开站点文件的配置
        m_doc->LoadFile(m_sitefile);
        if (m_doc->Error()) {
            printf("[%d,%d] %s", m_doc->ErrorRow(), m_doc->ErrorCol() , m_doc->ErrorDesc());
            return -1;
        }
		//找到root节点
        TiXmlElement *root = m_doc->RootElement();
        if (root == NULL) {
            printf("error\n");
            return -1;
        }

        _append_node(root, NULL);

        m_doc->Clear();
    }

    return -1;
}

int site_save()
{
    return -1;
}

GtkWidget *site_get_object()
{
    return m_object;
}
