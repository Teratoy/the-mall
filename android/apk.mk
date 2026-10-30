# The Mall as a debug APK for a phone (arm64), built with the Android NDK and
# SDK build-tools: no Gradle, no Java code. Run on the host, not in the
# toolbox, since signing needs Java:
#   make apk            builds build/TheMall-debug.apk
#   make install-apk    puts it on a phone plugged in with USB debugging on
#   make logcat         shows what the game prints there
# Plan and decisions: AI/android_plan.md.

SDK         ?= $(HOME)/Android/Sdk
# The newest NDK and build-tools installed, unless named.
NDK         ?= $(lastword $(sort $(wildcard $(SDK)/ndk/*)))
BUILD_TOOLS ?= $(lastword $(sort $(wildcard $(SDK)/build-tools/*)))
# The Android the APK is built against, and the oldest it runs on.
PLATFORM    ?= $(SDK)/platforms/android-36
MIN_SDK     := 26
TARGET_SDK  := 35
RAYLIB_TAG  := 5.5

TOOLCHAIN := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
CC        := $(TOOLCHAIN)/aarch64-linux-android$(MIN_SDK)-clang
CXX       := $(TOOLCHAIN)/aarch64-linux-android$(MIN_SDK)-clang++
AR        := $(TOOLCHAIN)/llvm-ar
STRIP     := $(TOOLCHAIN)/llvm-strip
GLUE      := $(NDK)/sources/android/native_app_glue

B       := build/android
RAYLIB  := $(B)/raylib
APK     := build/TheMall-debug.apk
# The standard Android debug key, kept outside build/ so a clean doesn't
# lose it: an APK signed with another key won't install over the last one.
KEYSTORE := $(HOME)/.android/debug.keystore

RAYLIB_SRC := rcore rshapes rtextures rtext rmodels raudio utils
RAYLIB_OBJ := $(RAYLIB_SRC:%=$(B)/raylib-obj/%.o)
RAYLIB_CFLAGS := -std=c99 -O2 -fPIC -w -D_GNU_SOURCE -DPLATFORM_ANDROID -DGRAPHICS_API_OPENGL_ES2 -I$(GLUE)

SRC := $(shell find src -name '*.cpp')
OBJ := $(SRC:src/%.cpp=$(B)/obj/%.o)
CXXFLAGS := -std=c++20 -O2 -g -fPIC -Wall -Wextra -MMD -MP -DPLATFORM_ANDROID -Isrc -I$(RAYLIB)/src -I$(GLUE)
LDLIBS   := -static-libstdc++ -landroid -llog -lEGL -lGLESv2 -lOpenSLES -lm

.PHONY: apk install-apk logcat

apk: $(APK)

$(RAYLIB)/src/raylib.h:
	@test -d "$(NDK)" || (echo "apk: no NDK in $(SDK)/ndk: install it from Android Studio's SDK Manager"; exit 1)
	git clone -q --depth 1 --branch $(RAYLIB_TAG) https://github.com/raysan5/raylib.git $(RAYLIB)

$(B)/raylib-obj/%.o: $(RAYLIB)/src/raylib.h
	@mkdir -p $(@D)
	$(CC) $(RAYLIB_CFLAGS) -c $(RAYLIB)/src/$*.c -o $@

$(B)/libraylib.a: $(RAYLIB_OBJ)
	$(AR) rcs $@ $^

$(B)/glue.o: $(RAYLIB)/src/raylib.h
	$(CC) -std=c99 -O2 -fPIC -w -c $(GLUE)/android_native_app_glue.c -o $@

$(B)/obj/%.o: src/%.cpp $(RAYLIB)/src/raylib.h
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# NativeActivity finds the game by ANativeActivity_onCreate, from the glue.
$(B)/libthemall.so: $(OBJ) $(B)/glue.o $(B)/libraylib.a
	$(CXX) -shared -o $@ $^ -u ANativeActivity_onCreate -Wl,--no-undefined -Wl,-z,max-page-size=16384 $(LDLIBS)

$(B)/res.zip: $(shell find android/res -type f)
	$(BUILD_TOOLS)/aapt2 compile --dir android/res -o $@

$(KEYSTORE):
	@mkdir -p $(@D)
	keytool -genkeypair -keystore $@ -storepass android -alias androiddebugkey -keypass android \
		-keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"

# The data files go in as assets under the same names the game opens on a
# computer; the library goes in stripped of its debug symbols.
$(APK): android/AndroidManifest.xml $(B)/res.zip $(B)/libthemall.so data/lines.txt data/names.txt | $(KEYSTORE)
	@command -v java > /dev/null || (echo "apk: signing needs Java: run make apk on the host, not in the toolbox"; exit 1)
	rm -rf $(B)/pkg && mkdir -p $(B)/pkg/assets/data $(B)/pkg/lib/arm64-v8a
	cp data/lines.txt data/names.txt $(B)/pkg/assets/data/
	$(STRIP) --strip-debug -o $(B)/pkg/lib/arm64-v8a/libthemall.so $(B)/libthemall.so
	$(BUILD_TOOLS)/aapt2 link -o $(B)/unsigned.apk --manifest android/AndroidManifest.xml -I $(PLATFORM)/android.jar \
		--min-sdk-version $(MIN_SDK) --target-sdk-version $(TARGET_SDK) --debug-mode -A $(B)/pkg/assets $(B)/res.zip
	cd $(B)/pkg && zip -q -r ../unsigned.apk lib
	$(BUILD_TOOLS)/zipalign -f -P 16 4 $(B)/unsigned.apk $(B)/aligned.apk
	$(BUILD_TOOLS)/apksigner sign --ks $(KEYSTORE) --ks-pass pass:android --key-pass pass:android \
		--ks-key-alias androiddebugkey --v4-signing-enabled false --out $@ $(B)/aligned.apk
	@echo "apk: $@ is ready: copy it to the phone and open it to install"

install-apk: $(APK)
	$(SDK)/platform-tools/adb install -r $(APK)
	$(SDK)/platform-tools/adb shell am start -n com.teratoy.themall/android.app.NativeActivity

logcat:
	$(SDK)/platform-tools/adb logcat -s raylib:V threaded_app:V libc:V DEBUG:V

-include $(OBJ:.o=.d)
