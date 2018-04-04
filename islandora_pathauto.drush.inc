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
  $settings = islandora_pathauto_pathauto('settings');
  $pattern_all_objects = variable_get('pathauto_' . $settings->module . '_pattern', $settings->patterndefault);
  // Determine what content models have a pattern and
  // only query objects with those particular models.
  if (isset($settings->patternitems)) {
    foreach ($settings->patternitems as $model => $label) {
      $pattern = variable_get('pathauto_' . $settings->module . '_' . $model . '_pattern', '');
      if (!empty($pattern)) {
        $models[] = $model;
      }
    }
  }
  if (empty($models)) {
    $models[] = 'fedora-system:FedoraObject-3.0';
  }

  foreach ($models as &$filter) {
    $filter = "sameTerm(?model, <info:fedora/$filter>)";
  }
  $filters['model'] = format_string('FILTER(!models)', array('!models' => implode(' || ', $models)));
  if (variable_get('islandora_namespace_restriction_enforced', FALSE)) {
    $namespace_array = islandora_get_allowed_namespaces();
    $namespace_sparql = implode('|', $namespace_array);
    $filters['ns'] = format_string('FILTER(regex(str(?object), "info:fedora/(!namespaces):"))', array('!namespaces' => $namespace_sparql));
  }

  $query = <<<EOQ
SELECT ?object ?cd ?title
FROM <#ri>
WHERE {
  ?object <fedora-model:label> ?title ;
  <info:fedora/fedora-system:def/model#createdDate> ?cd ;
  <fedora-model:hasModel> ?model ;
  <fedora-model:state> <fedora-model:Active> .
  !filters
}
ORDER BY ?cd
!limit
EOQ;

  drupal_static_reset('islandora_get_tuque_connection');
  $tuque = islandora_get_tuque_connection();
  if ($tuque) {
    $firstcreationdate;
    $lastcreationdate;
    if (isset($fromdate)) {
      $lastcreationdate = $fromdate;
    }
    $limit = 2500;
    $checked = 0;
    $added = 0;
    $cycles = 0;

    while (TRUE) {
      $cycles++;
      if (isset($lastcreationdate)) {
        $filters['cd'] = "FILTER(?cd > '{$lastcreationdate}'^^xsd:dateTime)";
      }
      else {
        $filters['cd'] = '';
      }

      $query_string = format_string($query, array(
        '!filters' => implode(' ', $filters),
        '!limit' => "LIMIT $limit"));
      $results = $tuque->repository->ri->sparqlQuery($query_string);
      if (empty($results)) {
        break;
      }
      else {
        foreach ($results as $result) {
          $pid = $result['object']['value'];
          if (!isset($firstcreationdate)) {
            $firstcreationdate = $result['cd']['value'];
          }
          $lastcreationdate = $result['cd']['value'];

          $source = "islandora/object/$pid";
   
          $checked++;

          $result = db_query('SELECT a.source FROM url_alias a WHERE a.source LIKE :source',
            array(':source' => $source));
          $c = $result->rowCount();

          if ($c == 0) {
            if ($isdryrun) {
              $added++;
              drush_log("Should add alias for $source", "notice"); 
            }
            else {
              //db_insert('url_alias')->fields(array('alias' => $alias, 'source' => $source, 'language' => 'und'))->execute();
              $object = islandora_object_load($pid);
              if ($object) {
                $alias = islandora_pathauto_create_alias($object, 'bulkupdate');
                if ($alias != "") {
                  $added++;
                }
              }
            }
          }
        }
      }
      if (($cycles % 50) === 0) {
        drupal_static_reset('islandora_get_tuque_connection');
        $tuque = islandora_get_tuque_connection();
      }
    }
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

