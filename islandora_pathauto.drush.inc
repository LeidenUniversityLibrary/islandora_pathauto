<?php

/**
 * @file
 * Drush integration for the Pathauto module.
 */

/**
 * Implements hook_drush_command().
 */
function islandora_pathauto_drush_command() {
  $items['islandora_aliases_complete'] = array(
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'description' => 'Update aliases of islandora objects.',
    'options' => array(
      'dryrun' => 'See what aliases are to be completed.',
      'fromdate' => 'The creation date of objects to start with. Use valid date (e.g. 2018-01-17 or 2018-01-17T13:30:00.000Z) or the value "last" to start from the last date the aliasses where created.'
    ),
  );

  return $items;
}

/**
 * implements drush_hook_COMMAND_validate()
 */
function drush_islandora_pathauto_islandora_aliases_complete_validate() {
  $fromdate = drush_get_option('fromdate');
  if (isset($fromdate)) {
    if ($fromdate !== 'last') {
      if (!preg_match('/^[12][0-9]{3}-[01][0-9]-[0-3][0-9](?:T[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\\.[0-9]{3}Z)?$/', $fromdate)) {
        return drush_set_error(t("Argument 'fromdate' is not valid, should have format YYYY-MM-DD or YYYY-MM-DDThh-mm-ss.qqqZ"));
      }
    }
  }
}

/**
 * implements drush_hook_COMMAND()
 */
function drush_islandora_pathauto_islandora_aliases_complete() {
  $isdryrun = drush_get_option('dryrun');
  $fromdate = drush_get_option('fromdate');
  if ($fromdate === 'last') {
    $fromdate = variable_get('islandora_pathauto_aliases_complete_last_date', '1900-01-01');
  }
  if (preg_match('/^[12][0-9]{3}-[01][0-9]-[0-3][0-9]$/', $fromdate) === 1) {
    $fromdate = $fromdate . 'T00:00:00.000Z';
  }

  batch_set(array(
    'title' => t('Islandora Pathauto cron bulk updating URL aliases'),
    'operations' => array(
      array('islandora_pathauto_bulk_batch_process_via_drush', array($fromdate, $isdryrun)),
    ),
  ));

  // This is a hack around the broken batch API.
  // https://drupal.org/node/638712#comment-2289138
  $batch =& batch_get();
  $batch['progressive'] = FALSE;
  drush_backend_batch_process();
}

/**
 * Batch operation for generating aliases for Islandora objects via drush.
 *
 * @param bool $use_cron_last
 *   Flag for using cron last time.
 * @param array $context
 *   Batch context.
 */
function islandora_pathauto_bulk_batch_process_via_drush($fromdate = NULL, $isdryrun = FALSE, &$context = array()) {
  if ($isdryrun) {
    $context['dryrun'] = TRUE;
  }
  islandora_pathauto_bulk_batch_process_from_date($fromdate, $context);

  if ($context['finished'] === 1) {
    $sandbox = $context['sandbox'];
    if (isset($sandbox['warning'])) {
      drush_log($sandbox['warning'], 'warning');
    }
    $checked = $sandbox['total_processed'];
    $added = $sandbox['total_alias'];
    $firstcreationdate = $sandbox['firstcd'];
    $lastcreationdate = $sandbox['cd'];
    drush_log(t("Checked !checked objects between @first and @last, !verb !added aliases.", array(
      '!checked' => $checked,
      '!added' => $added,
      '!verb' => $isdryrun?"missing":"added",
      '@first' => $firstcreationdate,
      '@last' => $lastcreationdate,
    )), "success");
    if (!$isdryrun) {
      variable_set('islandora_pathauto_aliases_complete_last_date', $lastcreationdate);
    }
  }
}
