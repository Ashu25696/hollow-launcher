package net.kdt.pojavlaunch.utils.jre;

import static net.kdt.pojavlaunch.Tools.NATIVE_LIB_DIR;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;

import net.kdt.pojavlaunch.AWTCanvasView;
import net.kdt.pojavlaunch.Tools;
import net.kdt.pojavlaunch.multirt.MultiRTUtils;
import net.kdt.pojavlaunch.multirt.Runtime;
import net.kdt.pojavlaunch.prefs.LauncherPreferences;
import net.kdt.pojavlaunch.utils.JREUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.TimeZone;

public class JavaRunner {
    public static void getCacioJavaArgs(List<String> javaArgList, boolean isJava8) {
        // Caciocavallo config AWT-enabled version
        javaArgList.add("-Djava.awt.headless=false");
        javaArgList.add("-Dcacio.managed.screensize=" + AWTCanvasView.AWT_CANVAS_WIDTH + "x" + AWTCanvasView.AWT_CANVAS_HEIGHT);
        javaArgList.add("-Dcacio.font.fontmanager=sun.awt.X11FontManager");
        javaArgList.add("-Dcacio.font.fontscaler=sun.font.FreetypeFontScaler");
        javaArgList.add("-Dswing.defaultlaf=javax.swing.plaf.metal.MetalLookAndFeel");
        if (isJava8) {
            javaArgList.add("-Dawt.toolkit=net.java.openjdk.cacio.ctc.CTCToolkit");
            javaArgList.add("-Djava.awt.graphicsenv=net.java.openjdk.cacio.ctc.CTCGraphicsEnvironment");
        } else {
            javaArgList.add("-Dawt.toolkit=com.github.caciocavallosilano.cacio.ctc.CTCToolkit");
            javaArgList.add("-Djava.awt.graphicsenv=com.github.caciocavallosilano.cacio.ctc.CTCGraphicsEnvironment");
            javaArgList.add("-Djava.system.class.loader=com.github.caciocavallosilano.cacio.ctc.CTCPreloadClassLoader");

            javaArgList.add("--add-exports=java.desktop/java.awt=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/java.awt.peer=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/sun.awt.image=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/sun.java2d=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/java.awt.dnd.peer=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/sun.awt=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/sun.awt.event=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/sun.awt.datatransfer=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.desktop/sun.font=ALL-UNNAMED");
            javaArgList.add("--add-exports=java.base/sun.security.action=ALL-UNNAMED");
            javaArgList.add("--add-opens=java.base/java.util=ALL-UNNAMED");
            javaArgList.add("--add-opens=java.desktop/java.awt=ALL-UNNAMED");
            javaArgList.add("--add-opens=java.desktop/sun.font=ALL-UNNAMED");
            javaArgList.add("--add-opens=java.desktop/sun.java2d=ALL-UNNAMED");
            javaArgList.add("--add-opens=java.base/java.lang.reflect=ALL-UNNAMED");
        }

        StringBuilder cacioClasspath = createCacioClasspath(isJava8);
        javaArgList.add(cacioClasspath.toString());
    }

    @NonNull
    private static StringBuilder createCacioClasspath(boolean isJava8) {
        StringBuilder cacioClasspath = new StringBuilder();
        cacioClasspath.append("-Xbootclasspath/").append(isJava8 ? "p" : "a");
        File cacioDir = new File(Tools.DIR_GAME_HOME + "/caciocavallo" + (isJava8 ? "" : "17"));
        File[] cacioFiles = cacioDir.listFiles();
        if (cacioFiles != null) {
            for (File file : cacioFiles) {
                if (file.getName().endsWith(".jar")) {
                    cacioClasspath.append(":").append(file.getAbsolutePath());
                }
            }
        }
        return cacioClasspath;
    }

    /**
     *  Gives an argument list filled with both the user args
     *  and the auto-generated ones (eg. the window resolution).
     * @return A list filled with args.
     */
    public static List<String> getJavaArgs(String runtimeHome, List<String> userArguments) {
        String resolvFile;
        resolvFile = new File(Tools.DIR_DATA,"resolv.conf").getAbsolutePath();

        userArguments.add(0, "-Xms"+LauncherPreferences.PREF_RAM_ALLOCATION+"M");
        userArguments.add(0, "-Xmx"+LauncherPreferences.PREF_RAM_ALLOCATION+"M");

        ArrayList<String> overridableArguments = new ArrayList<>(Arrays.asList(
                "-Djava.home=" + runtimeHome,
                "-Djava.io.tmpdir=" + Tools.DIR_CACHE.getAbsolutePath(),
                "-Djna.boot.library.path=" + NATIVE_LIB_DIR,
                "-Duser.home=" + Tools.DIR_GAME_HOME,
                "-Duser.language=" + System.getProperty("user.language"),
                "-Dos.name=Linux",
                "-Dos.version=Android-" + Build.VERSION.RELEASE,
                "-Dpojav.path.minecraft=" + Tools.DIR_GAME_NEW,
                "-Dpojav.path.private.account=" + Tools.DIR_ACCOUNT_NEW,
                "-Duser.timezone=" + TimeZone.getDefault().getID(),

                "-Dorg.lwjgl.vulkan.libname=libvulkan.so",
                //LWJGL 3 DEBUG FLAGS
                //"-Dorg.lwjgl.util.Debug=true",
                //"-Dorg.lwjgl.util.DebugFunctions=true",
                //"-Dorg.lwjgl.util.DebugLoader=true",
                // GLFW Stub width height
                "-Dglfwstub.initEgl=false",
                "-Dext.net.resolvPath=" +resolvFile,
                "-Dlog4j2.formatMsgNoLookups=true", //Log4j RCE mitigation
                "-Dfml.earlyprogresswindow=false", //Forge 1.14+ workaround
                "-Dloader.disable_forked_guis=true",
                "-Dsodium.checks.issue2561=false",
                "-Djdk.lang.Process.launchMechanism=FORK" // Default is POSIX_SPAWN which requires starting jspawnhelper, which doesn't work on Android
        ));
        List<String> additionalArguments = new ArrayList<>();
        for(String arg : overridableArguments) {
            String strippedArg = arg.substring(0,arg.indexOf('='));
            boolean add = true;
            for(String uarg : userArguments) {
                if(uarg.startsWith(strippedArg)) {
                    add = false;
                    break;
                }
            }
            if(add)
                additionalArguments.add(arg);
            else
                Log.i("ArgProcessor","Arg skipped: "+arg);
        }

        //Add all the arguments
        userArguments.addAll(additionalArguments);
        return userArguments;
    }

    private static File getVmPath(File runtimeHomeDir, String arch, String flavor) {
        if(arch != null) return new File(runtimeHomeDir, "lib/"+arch+"/"+flavor+"/libjvm.so");
        else return new File(runtimeHomeDir, "lib/"+flavor+"/libjvm.so");
    }

    private static String findVmPath(File runtimeHomeDir, String runtimeArch) {
        File finalPath;
        if((finalPath = getVmPath(runtimeHomeDir, null, "server")).exists()) return finalPath.getAbsolutePath();
        if((finalPath = getVmPath(runtimeHomeDir, null, "client")).exists()) return finalPath.getAbsolutePath();
        if((finalPath = getVmPath(runtimeHomeDir, runtimeArch, "server")).exists()) return finalPath.getAbsolutePath();
        if((finalPath = getVmPath(runtimeHomeDir, runtimeArch, "client")).exists()) return finalPath.getAbsolutePath();
        return null;
    }

    private static void preprocessUserArgs(List<String> args) {
        ListIterator<String> iterator = args.listIterator();
        while(iterator.hasNext()) {
            String arg = iterator.next();
            switch (arg) {
                case "--add-reads":
                case "--add-exports":
                case "--add-opens":
                case "--add-modules":
                case "--limit-modules":
                case "--module-path":
                case "--patch-module":
                case "--upgrade-module-path":
                    iterator.remove();
                    String argValue = iterator.next();
                    iterator.remove();
                    iterator.add(arg+"="+argValue);
                    break;
                case "-d32":
                case "-d64":
                case "-Xint":
                case "-XX:+UseTransparentHugePages":
                case "-XX:+UseLargePagesInMetaspace":
                case "-XX:+UseLargePages":
                case "-XX:ActiveProcessorCount":
                    iterator.remove();
                    break;
                default:
                    if(arg.startsWith("-Xms") || arg.startsWith("-Xmx")) iterator.remove();
            }

        }
    }

    public static void startJvm(Runtime runtime, List<String> userArgs, List<String> classpathEntries, String mainClass, List<String> applicationArgs) throws VMLoadException{
        File runtimeHomeDir = MultiRTUtils.getRuntimeHome(runtime.name);
        String vmPath = findVmPath(runtimeHomeDir, runtime.arch);

        preprocessUserArgs(userArgs);
        List<String> runtimeArgs = getJavaArgs(runtimeHomeDir.getAbsolutePath(), userArgs);
        getCacioJavaArgs(runtimeArgs,runtime.javaVersion == 8);

        runtimeArgs.add("-XX:ActiveProcessorCount=" + java.lang.Runtime.getRuntime().availableProcessors());

        JREUtils.initializeHooks();

        nativeLoadJVM(vmPath, runtimeArgs.toArray(new String[0]), classpathEntries.toArray(new String[0]), mainClass, applicationArgs.toArray(new String[0]));
    }

    public static native boolean nativeLoadJVM(String vmPath, String[] javaArgs, String[] classpath, String mainClass, String[] appArgs) throws VMLoadException;
    public static native void nativeSetupExit(Context context);
}
