'use strict';

// Include Gulp & Tools We'll Use
var gulp = require('gulp');
var $ = require('gulp-load-plugins')();
var del = require('del');
var runSequence = require('run-sequence');
var browserSync = require('browser-sync');
var manifest = require('gulp-manifest');
var reload = browserSync.reload;

var AUTOPREFIXER_BROWSERS = [
  'ie >= 10',
  'ie_mob >= 10',
  'ff >= 30',
  'chrome >= 34',
  'safari >= 7',
  'opera >= 23',
  'ios >= 7',
  'android >= 4.4',
  'bb >= 10'
];

// Lint JavaScript
gulp.task('jshint', function() {
  return gulp.src([
    'app/scripts/**/*.js',
    '!app/scripts/vendor/**/*.js',
    'gulpfile.js'])
    .pipe(reload({stream: true, once: true}))
    .pipe($.jshint())
    .pipe($.jshint.reporter('jshint-stylish'))
    .pipe($.if(!browserSync.active, $.jshint.reporter('fail')));
});

//Check JavaScript code style
gulp.task('jscs', function() {
  return gulp.src([
    'app/scripts/**/*.js',
    '!app/scripts/vendor/**/*.js',
    'gulpfile.js'])
    .pipe(reload({stream: true, once: true}))
    .pipe($.jscs());
});

// Copy All Files At The Root Level (app)
gulp.task('copy', function() {
  return gulp.src([
    'app/*',
    'app/images/**/*.*',
    '!app/*.html'
  ], { base: 'app', dot: true })
    .pipe(gulp.dest('dist'))
    .pipe($.size({title: 'copy'}));
});

// Compile and Automatically Prefix Stylesheets
gulp.task('styles', function() {
  return gulp.src([
    'app/styles/*.scss',
    'app/styles/**/*.css',
    'app/styles/components/components.scss'
  ])
    .pipe($.changed('styles', {extension: '.scss'}))
    .pipe($.rubySass({
      style: 'expanded',
      precision: 10
    }))
    .on('error', console.error.bind(console))
    .pipe($.autoprefixer({browsers: AUTOPREFIXER_BROWSERS}))
    .pipe(gulp.dest('.tmp/styles'))
    // Concatenate And Minify Styles
    .pipe($.if('*.css', $.csso()))
    .pipe(gulp.dest('dist/styles'))
    .pipe($.size({title: 'styles'}));
});

// Scan Your HTML For Assets & Optimize Them
gulp.task('html', function() {
  var assets = $.useref.assets({searchPath: '{.tmp,app}'});

  return gulp.src('app/**/*.html')
    .pipe(assets)
    // Concatenate And Minify JavaScript
    .pipe($.if('*.js', $.uglify({preserveComments: 'some'})))
    // Remove Any Unused CSS
    // Note: If not using the Style Guide, you can delete it from
    // the next line to only include styles your project uses.
    .pipe($.if('*.css', $.uncss({
      html: [
        'app/index.html'
      ]
    })))
    // Concatenate And Minify Styles
    // In case you are still using useref build blocks
    .pipe($.if('*.css', $.csso()))
    .pipe(assets.restore())
    .pipe($.useref())
    // Minify Any HTML
    .pipe($.if('*.html', $.minifyHtml()))
    // Output Files
    .pipe(gulp.dest('dist'))
    .pipe($.size({title: 'html'}));
});

gulp.task('replace:manifest', function() {
  gulp.src('dist/index.html')
    .pipe($.replace(/<html /, '<html manifest="manifest.appcache" '))
    .pipe(gulp.dest('dist'));
});

//Create offline manifest
gulp.task('manifest', ['replace:manifest'], function() {
  gulp.src(['dist/**/*.*'])
    .pipe(manifest({
      hash: true,
      preferOnline: true,
      network: ['http://*'],
      filename: 'manifest.appcache',
      exclude: 'manifest.appcache'
    }))
    .pipe(gulp.dest('dist'));
});

// Clean Output Directory
gulp.task('clean', del.bind(null, ['.tmp', 'dist/*']));

// Watch Files For Changes & Reload
gulp.task('serve', ['styles'], function() {
  browserSync({
    notify: false,
    server: ['.tmp', 'app']
  });

  gulp.watch(['app/**/*.html'], reload);
  gulp.watch(['app/styles/**/*.{scss,css}'], ['styles', reload]);
  gulp.watch(['app/scripts/**/*.js'], ['jshint', 'jscs']);
  gulp.watch(['app/images/**/*'], reload);
});

// Build and serve the output from the dist build
gulp.task('serve:dist', ['default'], function() {
  browserSync({
    notify: false,
    server: 'dist'
  });
});

// Build Production Files, the Default Task
gulp.task('default', ['clean'], function(cb) {
  runSequence('styles', ['jshint', 'jscs', 'html', 'copy'], 'manifest', cb);
});

// Load custom tasks from the `tasks` directory
try { require('require-dir')('tasks'); } catch (err) {}
