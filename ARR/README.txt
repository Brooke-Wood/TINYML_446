Contents
Note that the file names in this repository may not match names in the code

ARR/Models
>Teacher.tflite
>>Model used to train student. Recreated in Juptyer Notebook 
  from the same architecture as a model created in EdgeImpulse (LINK: )
>Student_int8.tflite
>>Created student model. Additional optimizations (pruning and quantization) have been implemented as well. 

ARR/Programs
>Demo_python.ipynb
>>Code used during the in class demo to send data to the arduino
>Demo_arduino.ino
>>Code flashed to the Arduino board for the in class demo. Receives and classfies sent image tensors.
>Student_Creation.ipynb
>>Code used to create and optimize final student model used in deployment. 
>Bulk_Preprocessor.ipynb
>>Used to process the original dataset into the cropped dataset
>Image_Preprocessor.ipynb
>>Used to preprocess a single image to make it ready for classification 

ARR/Data
(The original dataset was too large to upload to github. Find it here: https://www.kaggle.com/datasets/brookewooduw/resistor-photos-215 )
>Resistor_Data_Cropped.zip
>>images that have been edited by the bulk autocropper program.
  A handful of images that "failed" the process were left out
