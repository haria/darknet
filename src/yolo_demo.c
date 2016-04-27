#include "network.h"
#include "detection_layer.h"
#include "cost_layer.h"
#include "utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include <sys/time.h>

#ifdef OPENCV
#include "opencv/highgui.h"
#include "opencv2/imgproc/imgproc_c.h"
image ipl_to_image(IplImage* src);
void convert_yolo_detections(float *predictions, int classes, int num, int square, int side, int w, int h, float thresh, float **probs, box *boxes, int only_objectness);
void draw_yolo(image im, int num, float thresh, box *boxes, float **probs);

extern char *voc_names[];
extern image voc_labels[];

static float **probs;
static box *boxes;
static network net;
static image in   ;
static image in_s ;
static image det  ;
static image det_s;
static image disp ;
static CvCapture * cap;
static float fps = 0;
static float demo_thresh = 0;

void *fetch_in_thread(void *ptr)
{
    in = get_image_from_stream(cap);
    in_s = resize_image(in, net.w, net.h);
    return 0;
}

void *detect_in_thread(void *ptr)
{
    float nms = .4;

    detection_layer l = net.layers[net.n-1];
    float *X = det_s.data;
    float *predictions = network_predict(net, X);
    free_image(det_s);
    convert_yolo_detections(predictions, l.classes, l.n, l.sqrt, l.side, 1, 1, demo_thresh, probs, boxes, 0);
    if (nms > 0) do_nms(boxes, probs, l.side*l.side*l.n, l.classes, nms);
    printf("\033[2J");
    printf("\033[1;1H");
    printf("\nFPS:%.0f\n",fps);
    printf("Objects:\n\n");
    //#TODO
    draw_detections(det, l.side*l.side*l.n, demo_thresh, boxes, probs, voc_names, voc_labels, 2);
    return 0;
}

void demo_yolo(char *cfgfile, char *weightfile, float thresh, int cam_index, char *filename)
{
    demo_thresh = thresh;
    printf("YOLO demo\n");
    net = parse_network_cfg(cfgfile);
    if(weightfile){
        load_weights(&net, weightfile);
    }
    set_batch_network(&net, 1);

    srand(2222222);

    if(filename){
        cap = cvCaptureFromFile(filename);
    }else{
        cap = cvCaptureFromCAM(cam_index);
    }

    if(!cap) error("Couldn't connect to webcam.\n");
    cvNamedWindow("YOLO", CV_WINDOW_NORMAL); 
    cvResizeWindow("YOLO", 512, 512);

    detection_layer l = net.layers[net.n-1];
    int j;

    boxes = (box *)calloc(l.side*l.side*l.n, sizeof(box));
    probs = (float **)calloc(l.side*l.side*l.n, sizeof(float *));
    for(j = 0; j < l.side*l.side*l.n; ++j) probs[j] = (float *)calloc(l.classes, sizeof(float *));

    pthread_t fetch_thread;
    pthread_t detect_thread;

    fetch_in_thread(0);
    det = in;
    det_s = in_s;

    fetch_in_thread(0);
    detect_in_thread(0);
    disp = det;
    det = in;
    det_s = in_s;

    while(1){
        struct timeval tval_before, tval_after, tval_result;
        gettimeofday(&tval_before, NULL);
        if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
        if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
        show_image(disp, "YOLO");
        free_image(disp);
        char key = (char) cvWaitKey(10);
	    // Exit this loop on escape:
        if(key == 27|| key=='q' || key=='Q')
        	break;
        pthread_join(fetch_thread, 0);
        pthread_join(detect_thread, 0);

        disp  = det;
        det   = in;
        det_s = in_s;

        gettimeofday(&tval_after, NULL);
        timersub(&tval_after, &tval_before, &tval_result);
        float curr = 1000000.f/((long int)tval_result.tv_usec);
        fps = .9*fps + .1*curr;
    }
}
#else
void demo_yolo(char *cfgfile, char *weightfile, float thresh, int cam_index, char *filename){
    fprintf(stderr, "YOLO demo needs OpenCV for webcam images.\n");
}
#endif

