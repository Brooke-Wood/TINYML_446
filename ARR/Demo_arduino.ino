#include <TensorFlowLite.h>
#include "student_c.h"

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

constexpr int kTensorArenaSize = 50 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

const int IMG_SIZE    = 45 * 45 * 3;
const int NUM_CLASSES = 16;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  model = tflite::GetModel(student_c);

  static tflite::MicroErrorReporter micro_error_reporter;
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, &micro_error_reporter);
  interpreter = &static_interpreter;

  interpreter->AllocateTensors();
  input  = interpreter->input(0);
  output = interpreter->output(0);

  Serial.write('W');  // signal ready NOW, after everything is initialized
}

void loop() {
  static bool sentReady    = false;
  static int  bytesReceived = 0;

  if (Serial.available() == 0) return;

  char c = Serial.peek();
  if (c == 'P') {
    Serial.read();
    Serial.write('K');
    return;
  }

  // Accumulate image bytes
  while (Serial.available() > 0 && bytesReceived < IMG_SIZE) {
    input->data.int8[bytesReceived++] = (int8_t)Serial.read();
  }

  if (bytesReceived >= IMG_SIZE) {
    bytesReceived = 0;

    Serial.write('R');
    Serial.write('I');
    interpreter->Invoke();
    Serial.write('D');

    for (int i = 0; i < NUM_CLASSES; i++) {
      Serial.write((uint8_t)output->data.int8[i]);
    }
    Serial.write('W');  // signal ready for next image
  }
}