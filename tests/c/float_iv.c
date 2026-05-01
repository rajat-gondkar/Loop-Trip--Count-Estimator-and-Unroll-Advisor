void float_iv(float *arr) {
  for (float f = 0.0f; f < 1.0f; f += 0.1f) {
    arr[(int)(f * 10)] = f;
  }
}
