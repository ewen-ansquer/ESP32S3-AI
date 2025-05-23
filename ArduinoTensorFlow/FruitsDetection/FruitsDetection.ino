#include "tiny_model_int8.h"
#include <Chirale_TensorFlowLite.h>

// This TensorFlow Lite Micro Library for Arduino is not similar to standard
// Arduino libraries. These additional header files must be included.
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "esp_camera.h"
#include <esp_heap_caps.h>

// Globals pointers, used to address TensorFlow Lite components.
// Pointers are not usual in Arduino sketches, future versions of
// the library may change this...
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// There is no way to calculate this parameter
// the value is usually determined by trial and errors
// It is the dimension of the memory area used by the TFLite interpreter
// to store tensors and intermediate results
constexpr int kTensorArenaSize = 80000; // 64 KB


// Keep aligned to 16 bytes for CMSIS (Cortex Microcontroller Software Interface Standard)
// alignas(16) directive is used to specify that the array 
// should be stored in memory at an address that is a multiple of 16.
//alignas(16) uint8_t *tensor_arena;
uint8_t* tensor_arena = nullptr;


// All of these values are derived from the values used during model training,
// if you change your model you'll need to update these constants.
constexpr int kNumCols = 96;
constexpr int kNumRows = 96;
constexpr int kNumChannels = 3;

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#if defined(CAMERA_MODEL_XIAO_ESP32S3)
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39

#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

#else
#error "Camera model not selected"
#endif

/* Private variables ------------------------------------------------------- */
static bool is_initialised = false;
uint8_t *snapshot_buf; //points to the output of the capture

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,     //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;

// The name of this function is important for Arduino compatibility.
void setup() {
uint8_t* tensor_arena = (uint8_t*)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
  if(psramInit()){
        Serial.println("\nPSRAM is correctly initialized");
        Serial.println(ESP.getFreePsram());
}
  tensor_arena = (uint8_t*)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
  if (tensor_arena == nullptr) {
    Serial.println("Erreur : Échec de l'allocation du tensor_arena dans la PSRAM !");
    while(true); // Bloquer le programme si échec
  }
  // Initialize serial communications and wait for Serial Monitor to be opened
  Serial.begin(115200);
  while(!Serial);

  Serial.println("Initializing TensorFlow Lite Micro Interpreter...");

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(mobilenet_quant_int8_tflite);

  // Check if model and library have compatible schema version,
  // if not, there is a misalignement between TensorFlow version used
  // to train and generate the TFLite model and the current version of library
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model provided and schema version are not equal!");
    while(true); // stop program here
  }

  // This pulls in all the TensorFlow Lite operators.
  static tflite::AllOpsResolver resolver;



//tensor_arena = (uint8_t*)ps_malloc(kTensorArenaSize);

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  // if an error occurs, stop the program.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("AllocateTensors() failed");
    Serial.println("Tensor arena too small?");

    while(true); // stop program here
  }

  // Obtain pointers to the model's input and output tensors.
  input = interpreter->input(0);
  output = interpreter->output(0);

  if (ei_camera_init() == false) {
      Serial.println("Failed to initialize Camera!\r\n");
  }
  else {
      Serial.println("Camera initialized\r\n");
  }

  Serial.println("Initialization done.");
  Serial.println("");
}

void loop() {
  // Get image from provider.
  if (!ei_camera_capture(kNumCols, kNumRows, input->data.int8)) {
    Serial.println("Image capture failed.");
    return;
  }

  // Run inference, and report if an error occurs
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("Invoke failed!");
    return;
  }

  TfLiteTensor* output = interpreter->output(0);

  const char* labels[] = {"apple", "lemon", "kiwi"};
  const int num_labels = 3;

  int max_index = 0;
  float max_score = -1000.0;

  for (int i = 0; i < num_labels; i++) {
    int8_t quant_score = output->data.int8[i];
    float dequant_score = (quant_score - output->params.zero_point) * output->params.scale;

    Serial.print(labels[i]);
    Serial.print(": ");
    Serial.println(dequant_score);

    if (dequant_score > max_score) {
      max_score = dequant_score;
      max_index = i;
    }
  }

  Serial.print(">>> Prediction: ");
  Serial.println(labels[max_index]);
}


bool ei_camera_init(void) {

    if (is_initialised) return true;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1); // flip it back
      s->set_brightness(s, 1); // up the brightness just a bit
      s->set_saturation(s, 0); // lower the saturation
    }

#if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#elif defined(CAMERA_MODEL_ESP_EYE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_awb_gain(s, 1);
#endif

    is_initialised = true;
    return true;
}

void ei_camera_deinit(void) {

    //deinitialize the camera
    esp_err_t err = esp_camera_deinit();

    if (err != ESP_OK)
    {
        printf("Camera deinit failed\n");
        return;
    }

    is_initialised = false;
    return;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, int8_t *out_buf) {
  
    if (!is_initialised) {
        printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        printf("Camera capture failed\n");
        return false;
    }

   //bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, out_buf);

   for (uint32_t i = 0; i < fb->len; i++) {
    out_buf[i] = fb->buf[i] - 128;
   }

   esp_camera_fb_return(fb);

   //if(!converted){
   //    printf("Conversion failed\n");
   //    return false;
   //}

    return true;
}