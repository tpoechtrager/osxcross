namespace {
#ifdef _WIN32
int setenv(const char *name, const char *value, int overwrite) {
  std::string buf;
  (void)overwrite; // TODO

  buf = name;
  buf += '=';
  buf += value;

  return putenv(buf.c_str());
}
#endif
}
