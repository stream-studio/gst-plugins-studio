#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

static GMainLoop *loop;

typedef struct _StudioData {
  GstElement *pipeline;           /* Our one and only pipeline */
  GstElement *engine;
} StudioData;


static void
start_stream (GtkWidget *widget,
             StudioData *data)
{
  gboolean ret;
  g_print ("Hello World\n");
}



static void
stop_stream (GtkWidget *widget,
             StudioData *data)
{
  gboolean ret;
  g_print ("Hello World\n");
}



static void
start_record (GtkWidget *widget,
             StudioData *data)
{
  gboolean ret;
  g_signal_emit_by_name(data->engine, "start-record", "test.mp4", &ret);

}



static void
stop_record (GtkWidget *widget,
             StudioData *data)
{
  gboolean ret;
  g_signal_emit_by_name(data->engine, "stop-record", &ret);
}


static void
activate (GtkApplication *app,
          StudioData *data)
{
  GtkWidget *window;
  GtkWidget *buttonStartStream;
  GtkWidget *buttonStartRecord;
  GtkWidget *buttonStopRecord;
  GtkWidget *buttonStopStream;

  GtkWidget *button_box;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

  button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (window), button_box);

  buttonStartStream = gtk_button_new_with_label ("Start Stream");
  g_signal_connect (buttonStartStream, "clicked", G_CALLBACK (start_stream), data);
  gtk_container_add (GTK_CONTAINER (button_box), buttonStartStream);

  buttonStopStream = gtk_button_new_with_label ("Stop Stream");
  g_signal_connect (buttonStopStream, "clicked", G_CALLBACK (stop_stream), data);
  gtk_container_add (GTK_CONTAINER (button_box), buttonStopStream);

  buttonStartRecord = gtk_button_new_with_label ("Start Record");
  g_signal_connect (buttonStartRecord, "clicked", G_CALLBACK (start_record), data);
  gtk_container_add (GTK_CONTAINER (button_box), buttonStartRecord);


  buttonStopRecord = gtk_button_new_with_label ("Start Record");
  g_signal_connect (buttonStopRecord, "clicked", G_CALLBACK (stop_record), data);
  gtk_container_add (GTK_CONTAINER (button_box), buttonStopRecord);

  gtk_widget_show_all (window);
}


int main(int argc, char *argv[]){
    StudioData data;
    memset (&data, 0, sizeof (data));

    gtk_init (&argc, &argv);
    gst_init(&argc, &argv);
    


    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    data.pipeline = gst_pipeline_new("enginebin");
    data.engine = gst_element_factory_make("enginebin", "engine");
    gst_bin_add(GST_BIN(data.pipeline), data.engine);

    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    int status;
    GtkApplication *app;
    app = gtk_application_new ("com.stream.studio", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), &data);

    status = g_application_run (G_APPLICATION (app), argc, argv);
    gtk_main ();

    g_object_unref (app);
    

    
    return status;
}